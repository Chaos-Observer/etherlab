classdef baumer_ecatencoder < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function updateDC()
            slave = EtherCATSlave.findSlave(get_param(gcbh,'model'), ...
                                            baumer_ecatencoder.models);
            EtherCATSlaveBlock.updateDCVisibility(slave{4});
        end

        %====================================================================
        function rv = configure(model, direction, dc_config)
            slave = EtherCATSlave.findSlave(model,baumer_ecatencoder.models);

            rv.SlaveConfig.vendor = hex2dec('516');
            rv.SlaveConfig.description = slave{1};
            rv.SlaveConfig.product = slave{2};
            rv.SlaveConfig.revision = slave{3};

            rv.SlaveConfig.sm = baumer_ecatencoder.sm(1);

            rv.PortConfig.output = struct('pdo', [0,0,0,0], ...
                                          'pdo_data_type', uint(32));

            rv.SlaveConfig.sdo = {hex2dec('6000'),0,16,direction + 4};

            if slave{4} && dc_config(1) > 1
                if dc_config(1) > 2
                    rv.SlaveConfig.dc = dc_config(2:end);
                else
                    dc = baumer_ecatencoder.dc;
                    rv.SlaveConfig.dc = dc(dc_config(1),:);
                end
            end
        end

        %====================================================================
        function test(f)
            ei = EtherCATInfo(f);
            for i = 1:size(baumer_ecatencoder.models,1)
                fprintf('Testing %s\n', baumer_ecatencoder.models{i,1});
                rv = baumer_ecatencoder.configure(...
                        baumer_ecatencoder.models{i,1},i&1,2:12);
                slave = ei.getSlave(baumer_ecatencoder.models{i,2},...
                        'revision',baumer_ecatencoder.models{i,3});
                slave{1}.testConfig(rv.SlaveConfig);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant, Access = private)
        sm = {{3, 1, {{hex2dec('1A00') [hex2dec('6004') 0 32]}}}};
        dc = [             0,  0,0,0,0,0,0,0,0,0;
                hex2dec('300'),0,0,0,0,0,0,0,0,0];

    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant)

        %   Model       ProductCode Revision DC
        models = {...
            'BT ATD4',     1,          2, false;
            'BT ATD2',     2,          2,  true;
            'BT ATD2_PoE', 3,          2,  true;
            };
    end  % properties
end
