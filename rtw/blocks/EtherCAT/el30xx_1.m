classdef el30xx_1 < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function rv = configure(model,status,vector,scale,filter)
            % filter = [mode,opmode,fastop]

            slave = EtherCATSlave.findSlave(model,el30xx_1.models);

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = model;
            rv.SlaveConfig.product  = slave{2};
            rv.function = slave{4};

            pdo_count = str2double(model(6));

            if status
                pdo_idx = 9;
                value_idx = 9;
            else
                pdo_idx = 1;
                value_idx = 0;
            end

            pdo = el30xx_1.pdo;
            rv.SlaveConfig.sm = {{3,1, arrayfun(@(i) pdo(i,:), ...
                                         pdo_idx:(pdo_idx+pdo_count-1), ...
                                         'UniformOutput', false)}};

            pdo = repmat([0,0,value_idx,0],pdo_count,1);
            pdo(:,2) = 0:pdo_count-1;

            rv.PortConfig.output = ...
                el30xx_1.configurePorts('Ch.',pdo,sint(16),vector,scale);

            if status
                pdo(:,3) = 4;

                if vector
                    n = 1;
                else
                    n = pdo_count;
                end
                
                rv.PortConfig.output(end+(1:n)) = el30xx_1.configurePorts(...
                        'St.',pdo,uint(1),vector,isa(scale,'struct'));
            end

            % Hardware filter
            rv.SlaveConfig.sdo = {
                hex2dec('8000'),hex2dec(' 6'),  8, double(filter(1) > 1);
                hex2dec('8000'),hex2dec('15'), 16, max(0,filter(1)-2);
            };

            if filter(1) == 1
                % SyncMode 1C33:1
                % 1C33:1.0 = Synchronization Mode: 0=FreeRun, 1=SM2
                % 1C33:1.15 = FastOp mode
                % These are only available if no filter mode is selected
                rv.SlaveConfig.sdo(end+1,:) = ...
                        {hex2dec('1C33'), 1, 16, filter(3)*2^15+(filter(2)-1)};
            end

            % Todo FAST Mode (1C33:01)


        end

        %====================================================================
        function test(p)
            ei = EtherCATInfo(fullfile(p,'Beckhoff EL30xx.xml'));
            for i = 1:size(el30xx_1.models,1)
                fprintf('Testing %s\n', el30xx_1.models{i,1});
                rv = el30xx_1.configure(el30xx_1.models{i,1},0,i&1,...
                        EtherCATSlave.configureScale(2^15,''),1:3);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);

                rv = el30xx_1.configure(el30xx_1.models{i,1},1,i&1,...
                        EtherCATSlave.configureScale(2^15,'6'),2:4);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant)

        %              PdoEntry       EntryIdx
        pdo = { hex2dec('1a01'), [hex2dec('6000'), 17, 16];
                hex2dec('1a03'), [hex2dec('6010'), 17, 16];
                hex2dec('1a05'), [hex2dec('6020'), 17, 16];
                hex2dec('1a07'), [hex2dec('6030'), 17, 16];
                hex2dec('1a09'), [hex2dec('6040'), 17, 16];
                hex2dec('1a0b'), [hex2dec('6050'), 17, 16];
                hex2dec('1a0d'), [hex2dec('6060'), 17, 16];
                hex2dec('1a0f'), [hex2dec('6070'), 17, 16];

                % With status
                hex2dec('1a00'), [hex2dec('6000') ,  1,  1;      %7
                                  hex2dec('6000') ,  2,  1;
                                  hex2dec('6000') ,  3,  2;
                                  hex2dec('6000') ,  5,  2;
                                  hex2dec('6000') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  6;
                                  hex2dec('1800') ,  7,  1;
                                  hex2dec('1800') ,  9,  1;
                                  hex2dec('6000') , 17, 16];
                hex2dec('1a02'), [hex2dec('6010') ,  1,  1;      %8
                                  hex2dec('6010') ,  2,  1;
                                  hex2dec('6010') ,  3,  2;
                                  hex2dec('6010') ,  5,  2;
                                  hex2dec('6010') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  6;
                                  hex2dec('1802') ,  7,  1;
                                  hex2dec('1802') ,  9,  1;
                                  hex2dec('6010') , 17, 16];
                hex2dec('1a04'), [hex2dec('6020') ,  1,  1;      %9
                                  hex2dec('6020') ,  2,  1;
                                  hex2dec('6020') ,  3,  2;
                                  hex2dec('6020') ,  5,  2;
                                  hex2dec('6020') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  6;
                                  hex2dec('1804') ,  7,  1;
                                  hex2dec('1804') ,  9,  1;
                                  hex2dec('6020') , 17, 16];
                hex2dec('1a06'), [hex2dec('6030') ,  1,  1;      %10
                                  hex2dec('6030') ,  2,  1;
                                  hex2dec('6030') ,  3,  2;
                                  hex2dec('6030') ,  5,  2;
                                  hex2dec('6030') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  6;
                                  hex2dec('1806') ,  7,  1;
                                  hex2dec('1806') ,  9,  1;
                                  hex2dec('6030') , 17, 16];
                hex2dec('1a08'), [hex2dec('6040') ,  1,  1;      %7
                                  hex2dec('6040') ,  2,  1;
                                  hex2dec('6040') ,  3,  2;
                                  hex2dec('6040') ,  5,  2;
                                  hex2dec('6040') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  6;
                                  hex2dec('1808') ,  7,  1;
                                  hex2dec('1808') ,  9,  1;
                                  hex2dec('6040') , 17, 16];
                hex2dec('1a0a'), [hex2dec('6050') ,  1,  1;      %8
                                  hex2dec('6050') ,  2,  1;
                                  hex2dec('6050') ,  3,  2;
                                  hex2dec('6050') ,  5,  2;
                                  hex2dec('6050') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  6;
                                  hex2dec('180a') ,  7,  1;
                                  hex2dec('180a') ,  9,  1;
                                  hex2dec('6050') , 17, 16];
                hex2dec('1a0c'), [hex2dec('6060') ,  1,  1;      %9
                                  hex2dec('6060') ,  2,  1;
                                  hex2dec('6060') ,  3,  2;
                                  hex2dec('6060') ,  5,  2;
                                  hex2dec('6060') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  6;
                                  hex2dec('180c') ,  7,  1;
                                  hex2dec('180c') ,  9,  1;
                                  hex2dec('6060') , 17, 16];
                hex2dec('1a0e'), [hex2dec('6070') ,  1,  1;      %10
                                  hex2dec('6070') ,  2,  1;
                                  hex2dec('6070') ,  3,  2;
                                  hex2dec('6070') ,  5,  2;
                                  hex2dec('6070') ,  7,  1;
                                                0 ,  0,  1;
                                                0 ,  0,  6;
                                  hex2dec('180e') ,  7,  1;
                                  hex2dec('180e') ,  9,  1;
                                  hex2dec('6070') , 17, 16];
        };


        %   Model   ProductCode           Function NoStatus WithStatus;
        models = {
          'EL3001',      hex2dec('0bb93052'), hex2dec('00120000'), '+-10V' ;
          'EL3002',      hex2dec('0bba3052'), hex2dec('00120000'), '+-10V' ;
          'EL3004',      hex2dec('0bbc3052'), hex2dec('00120000'), '+-10V' ;
          'EL3008',      hex2dec('0bc03052'), hex2dec('00120000'), '+-10V' ;
          'EL3011',      hex2dec('0bc33052'), hex2dec('00100000'), '0-20mA';
          'EL3012',      hex2dec('0bc43052'), hex2dec('00100000'), '0-20mA';
          'EL3014',      hex2dec('0bc63052'), hex2dec('00100000'), '0-20mA';
          'EL3021',      hex2dec('0bcd3052'), hex2dec('00100000'), '4-20mA';
          'EL3022',      hex2dec('0bce3052'), hex2dec('00100000'), '4-20mA';
          'EL3024',      hex2dec('0bd03052'), hex2dec('00100000'), '4-20mA';
          'EL3041',      hex2dec('0be13052'), hex2dec('00110000'), '0-20mA';
          'EL3042',      hex2dec('0be23052'), hex2dec('00110000'), '0-20mA';
          'EL3044',      hex2dec('0be43052'), hex2dec('00110000'), '0-20mA';
          'EL3048',      hex2dec('0be83052'), hex2dec('00110000'), '0-20mA';
          'EL3051',      hex2dec('0beb3052'), hex2dec('00110000'), '4-20mA';
          'EL3052',      hex2dec('0bec3052'), hex2dec('00110000'), '4-20mA';
          'EL3054',      hex2dec('0bee3052'), hex2dec('00110000'), '4-20mA';
          'EL3058',      hex2dec('0bf23052'), hex2dec('00110000'), '4-20mA';
          'EL3061',      hex2dec('0bf53052'), hex2dec('00110000'), '0-10V' ;
          'EL3062',      hex2dec('0bf63052'), hex2dec('00110000'), '0-10V' ;
          'EL3062-0030', hex2dec('0bf63052'), hex2dec('0011001e'), '0-30V' ;
          'EL3064',      hex2dec('0bf83052'), hex2dec('00110000'), '0-10V' ;
          'EL3068',      hex2dec('0bfc3052'), hex2dec('00110000'), '0-10V' ;
        };
    end
end
