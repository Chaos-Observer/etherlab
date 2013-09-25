classdef ep43xx_1 < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function rv = configure(model,status,vector,tx_scale,rx_scale,dc,filter,type)
            slave = ep43xx_1.findSlave(model,ep43xx_1.models);

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = model;
            rv.SlaveConfig.product = slave{2};

            pdo_count = 2;

            rv.SlaveConfig.sm = ep43xx_1.sm;

            rx_pdo = repmat([0,0,0,0],pdo_count,1);
            rx_pdo(:,2) = 0:pdo_count-1;
            rv.PortConfig.input  = ep43xx_1.configurePorts('O',...
                                rx_pdo,sint(16),vector,rx_scale);

            if status
                rv.SlaveConfig.sm{2}{3} = ep43xx_1.status_pdo;
                tx_pdo1 = repmat([1,0,10,0],pdo_count,1);
                tx_pdo1(:,2) = 0:pdo_count-1;

                tx_pdo2 = tx_pdo1;
                tx_pdo2(:,3) = 4;
            else
                tx_pdo1 = repmat([1,0,0,0],pdo_count,1);
                tx_pdo1(:,2) = 0:pdo_count-1;
            end

            rv.PortConfig.output  = ep43xx_1.configurePorts('Value',...
                                tx_pdo1,sint(16),vector,tx_scale);

            if status
                rv.PortConfig.output(end+1) = ep43xx_1.configurePorts(...
                        'Status',tx_pdo2,uint(1),vector,isa(tx_scale,'struct'));
            end

            if dc(1) > 2
                rv.SlaveConfig.dc = dc(2:end);
            else
                rv.SlaveConfig.dc = ep43xx_1.dc{dc(1),2};
            end

            rv.SlaveConfig.sdo = {
                hex2dec('8000'),hex2dec( '6'), 8,double(filter > 1);
                hex2dec('8000'),hex2dec('15'),16,   max(0,filter-2);
                hex2dec('F000'),hex2dec( '1'),16,         type(1)-1;
                hex2dec('F000'),hex2dec( '2'),16,         type(2)-1;
                hex2dec('F000'),hex2dec( '3'),16,         type(3)-1;
                hex2dec('F000'),hex2dec( '4'),16,         type(4)-1;
            };

        end
 
        %====================================================================
        function test(p)
            ei = EtherCATInfo(fullfile(p,'Beckhoff EP4xxx.xml'));
            for i = 1:size(ep43xx_1.models,1)
                fprintf('Testing %s\n', ep43xx_1.models{i,1});
                rv = ep43xx_1.configure(ep43xx_1.models{i,1},0,i&1,[],[],1,1,1:4);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);

                rv = ep43xx_1.configure(ep43xx_1.models{i,1},1,i&1,[],[],1,2,1:4);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant, Access = private)
        % directly write the syncmanager configuration
        sm = {{2, 0, { {hex2dec('1600') [hex2dec('7020'),17,16]},
                       {hex2dec('1601') [hex2dec('7030'),17,16]}}},
              {3, 1, { {hex2dec('1a01') [hex2dec('6000'),17,16]},
                       {hex2dec('1a03') [hex2dec('6010'),17,16]}}}};

        status_pdo = { {hex2dec('1a00') [hex2dec('6000'), 1, 1;
                                         hex2dec('6000'), 2, 1;
                                         hex2dec('6000'), 3, 2;
                                         hex2dec('6000'), 5, 2;
                                         hex2dec('6000'), 7, 1;
                                         0              , 0, 1;
                                         0              , 0, 5;
                                         hex2dec('6000'),14, 1;
                                         hex2dec('6000'),15, 1;
                                         hex2dec('6000'),16, 1;
                                         hex2dec('6000'),17,16]},
                       {hex2dec('1a02') [hex2dec('6010'), 1, 1;
                                         hex2dec('6010'), 2, 1;
                                         hex2dec('6010'), 3, 2;
                                         hex2dec('6010'), 5, 2;
                                         hex2dec('6010'), 7, 1;
                                         0              , 0, 1;
                                         0              , 0, 5;
                                         hex2dec('6010'),14, 1;
                                         hex2dec('6010'),15, 1;
                                         hex2dec('6010'),16, 1;
                                         hex2dec('6010'),17,16]}};

        dc = {'FreeRun/SM-Synchron', [0, 0, 0,     0, 0, 0, 0, 1,     0, 0];
              'DC-Synchron',...
                        [hex2dec('700'), 0, 1, -5000, 0, 0, 0, 1, 80000, 0]};
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant)
        %   Model     ProductCode          Revision
        models = {
          'EP4374-0002',  hex2dec('11164052'), hex2dec('00100002')
        };
    end
end
