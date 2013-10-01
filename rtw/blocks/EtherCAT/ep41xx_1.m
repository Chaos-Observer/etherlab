classdef ep41xx_1 < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function rv = configure(model,vector,scale,dc,type)
            slave = EtherCATSlave.findSlave(model,ep41xx_1.models);

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = model;
            rv.SlaveConfig.product = slave{2};

            % PdoCount is needed to configure SDO
            pdo_count = str2double(model(6));

            rv.SlaveConfig.sm = ep41xx_1.sm;

            % Value output
            pdo = zeros(pdo_count,4);
            pdo(:,2) = 0:pdo_count-1;
            rv.PortConfig.input = ep41xx_1.configurePorts('Ch.',...
                           pdo,sint(16),vector,scale);
                      
            % Distributed clock
            if dc(1) > 2
                rv.SlaveConfig.dc = dc(2:11);
            else
                rv.SlaveConfig.dc = ep31xx_1.dc{dc(1),2};
            end

            basetype = [0,1,2,6];
            types = basetype(type);
            types = types(1:pdo_count)';

            rv.SlaveConfig.sdo = num2cell(horzcat(...
                hex2dec('F800')*ones(pdo_count,1),...
                (1:pdo_count)',...
                16*ones(pdo_count,1),...
                types));

        end
 
        %====================================================================
        function test(p)
            ei = EtherCATInfo(fullfile(p,'Beckhoff EP4xxx.xml'));
            for i = 1:size(ep41xx_1.models,1)
                fprintf('Testing %s\n', ep41xx_1.models{i,1});
                rv = ep41xx_1.configure(ep41xx_1.models{i,1},i&1,...
                        EtherCATSlave.configureScale(2^15,''),1,1:4);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);

                rv = ep41xx_1.configure(ep41xx_1.models{i,1},~(i&1),...
                        EtherCATSlave.configureScale(2^15,'6'),1,1:4);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);
            end
        end
   end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant, Access = private)

        sm = {...
                {2, 0, { {hex2dec('1600') [hex2dec('7000'), 17, 16]},...
                         {hex2dec('1601') [hex2dec('7010'), 17, 16]},...
                         {hex2dec('1602') [hex2dec('7020'), 17, 16]},...
                         {hex2dec('1603') [hex2dec('7030'), 17, 16]}}}...
        };

        dc = {'Free Run', [         0,   0, 0,     0, 0, 0, 0, 1,      0, 0];
              'DC-Synchron', ...
                        [hex2dec('730'), 0, 1,     0, 0, 0, 0, 1, 140000, 0];
        };

    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant)
        %   Model       ProductCode          Revision          HasOutput
        models = {
          'EP4174-0002', hex2dec('104e4052'), hex2dec('00110002'); ...
        };
    end
end
