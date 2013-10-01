classdef el1xxx < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function rv = configure(model,vector)
            slave = EtherCATSlave.findSlave(model,el1xxx.models);

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = model;
            rv.SlaveConfig.product  = slave{2};

            pdo_count = slave{5};

            pdo = repmat(0, pdo_count, 4);
            pdo(:,2) = 0:pdo_count-1;

            pdo_list = el1xxx.pdo;
            rv.SlaveConfig.sm = {{0,1, arrayfun(@(i) pdo_list(i,:), ...
                                         pdo(:,2)' + slave{4}, ...
                                         'UniformOutput', false)}};

            rv.PortConfig.output = ...
                el1xxx.configurePorts('D',pdo,uint(1),vector);

        end

        %====================================================================
        function test(p)
            ei = EtherCATInfo(fullfile(p,'Beckhoff EL1xxx.xml'));
            for i = 1:size(el1xxx.models,1)
                fprintf('Testing %s\n', el1xxx.models{i,1});
                rv = el1xxx.configure(el1xxx.models{i,1},i&1);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant)

        %              PdoEntry       EntryIdx, SubIdx, Bitlen
        pdo = { hex2dec('1a00'), [hex2dec('3101'), 1, 1];
                hex2dec('1a01'), [hex2dec('3101'), 2, 1];
                hex2dec('1a02'), [hex2dec('3101'), 3, 1];
                hex2dec('1a03'), [hex2dec('3101'), 4, 1];
                hex2dec('1a00'), [hex2dec('6000'), 1, 1];
                hex2dec('1a01'), [hex2dec('6010'), 1, 1];
                hex2dec('1a02'), [hex2dec('6020'), 1, 1];
                hex2dec('1a03'), [hex2dec('6030'), 1, 1];
                hex2dec('1a04'), [hex2dec('6040'), 1, 1];
                hex2dec('1a05'), [hex2dec('6050'), 1, 1];
                hex2dec('1a06'), [hex2dec('6060'), 1, 1];
                hex2dec('1a07'), [hex2dec('6070'), 1, 1];
                hex2dec('1a08'), [hex2dec('6080'), 1, 1];
                hex2dec('1a09'), [hex2dec('6090'), 1, 1];
                hex2dec('1a0a'), [hex2dec('60a0'), 1, 1];
                hex2dec('1a0b'), [hex2dec('60b0'), 1, 1];
                hex2dec('1a0c'), [hex2dec('60c0'), 1, 1];
                hex2dec('1a0d'), [hex2dec('60d0'), 1, 1];
                hex2dec('1a0e'), [hex2dec('60e0'), 1, 1];
                hex2dec('1a0f'), [hex2dec('60f0'), 1, 1];
                hex2dec('1a00'), [hex2dec('6000'), 1, 1];
                hex2dec('1a01'), [hex2dec('6000'), 2, 1]};

        %   Model   ProductCode              Function   IndexWithStatus;
        models = {...
            'EL1002', hex2dec('03ea3052'), hex2dec('00100000'), 5,  2;
            'EL1004', hex2dec('03ec3052'), hex2dec('00100000'), 5,  4;
            'EL1004-0010', ...
                      hex2dec('03ec3052'), hex2dec('0010000A'), 1,  4;
            'EL1008', hex2dec('03f03052'), hex2dec('00100000'), 5,  8;
            'EL1012', hex2dec('03f43052'), hex2dec('00100000'), 5,  2;
            'EL1014', hex2dec('03f63052'), hex2dec('00100000'), 5,  4;
            'EL1018', hex2dec('03fa3052'), hex2dec('00100000'), 5,  8;
            'EL1024', hex2dec('04003052'), hex2dec('00100000'), 5,  4;
            'EL1034', hex2dec('040a3052'), hex2dec('00100000'), 5,  4;
            'EL1084', hex2dec('043c3052'), hex2dec('00100000'), 5,  4;
            'EL1088', hex2dec('04403052'), hex2dec('00100000'), 5,  8;
            'EL1094', hex2dec('04463052'), hex2dec('00100000'), 5,  4;
            'EL1098', hex2dec('044a3052'), hex2dec('00100000'), 5,  8;
            'EL1104', hex2dec('04503052'), hex2dec('00100000'), 5,  4;
            'EL1114', hex2dec('045a3052'), hex2dec('00100000'), 5,  4;
            'EL1124', hex2dec('04643052'), hex2dec('00100000'), 5,  4;
            'EL1134', hex2dec('046e3052'), hex2dec('00100000'), 5,  4;
            'EL1144', hex2dec('04783052'), hex2dec('00100000'), 5,  4;
            'EL1202', hex2dec('04b23052'), hex2dec('00100000'),21,  2;
            'EL1702', hex2dec('06a63052'), hex2dec('00100000'), 5,  2;
            'EL1712', hex2dec('06b03052'), hex2dec('00100000'), 5,  2;
            'EL1722', hex2dec('06ba3052'), hex2dec('00100000'), 5,  2;
            'EL1804', hex2dec('070c3052'), hex2dec('00100000'), 5,  4;
            'EL1808', hex2dec('07103052'), hex2dec('00100000'), 5,  8;
            'EL1809', hex2dec('07113052'), hex2dec('00100000'), 5, 16;
            'EL1814', hex2dec('07163052'), hex2dec('00100000'), 5,  4;
            'EL1819', hex2dec('071b3052'), hex2dec('00100000'), 5, 16;
            'EL1862', hex2dec('07463052'), hex2dec('00100000'), 5, 16;
            'EL1872', hex2dec('07503052'), hex2dec('00100000'), 5, 16;
            'EL1889', hex2dec('07613052'), hex2dec('00100000'), 5, 16;
            'EL1014-0000-0000', ...
                      hex2dec('03f63052'), hex2dec('00000000'), 1,  4;
        };

    end
end
