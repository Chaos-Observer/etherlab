classdef el515x < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function rv = excludePdo(model,pdoIdx)
            slave = el515x.findSlave(model,el515x.models);
            pdoIdx = hex2dec(pdoIdx);

            pdo_list = el515x.pdo;
            i = [pdo_list{[slave{[4,5]}],1}] == pdoIdx;
            rv = ['x', dec2hex(pdo_list{i,2}) ];
        end

        %====================================================================
        function rv = configure(model,vector)
            slave = el515x.findSlave(model,el515x.models);

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = model;
            rv.SlaveConfig.product  = slave{2};

            pdo_list = el515x.pdo;
            rv.SlaveConfig.sm = {{2,0, arrayfun(@(i) pdo_list(i,[1,3]), ...
                                         slave{4}, ...
                                         'UniformOutput', false)};
                                 {3,1, arrayfun(@(i) pdo_list(i,[1,3]), ...
                                         slave{5}, ...
                                         'UniformOutput', false)}};

            rv.PortConfig.output = [];
%            rv.PortConfig.output = ...
%                el515x.configurePorts('D',pdo,uint(1),vector);

        end

        %====================================================================
        function test(p)
            ei = EtherCATInfo(fullfile(p,'Beckhoff EL5xxx.xml'));
            for i = 1:size(el515x.models,1)
                fprintf('Testing %s\n', el515x.models{i,1});
                rv = el515x.configure(el515x.models{i,1},i&1);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant, Access = private)

        %              PdoEntry       EntryIdx, SubIdx, Bitlen
        pdo = {
            hex2dec('1600'), hex2dec('1601'), [ hex2dec('7000'), 1, 1, 1;
                                                hex2dec('7000'), 2, 1, 1;
                                                hex2dec('7000'), 3, 1, 1;
                                                hex2dec('7000'), 4, 1, 1;
                                                0              , 0, 4, 0;
                                                0              , 0, 8, 0;
                                                hex2dec('7000'),17,32, 1];
            hex2dec('1601'), hex2dec('1600'), [ hex2dec('7000'), 1, 1, 1;
                                                hex2dec('7000'), 2, 1, 1;
                                                hex2dec('7000'), 3, 1, 1;
                                                hex2dec('7000'), 4, 1, 1;
                                                0              , 0, 4, 0;
                                                0              , 0, 8, 0;
                                                hex2dec('7000'),17,16, 1];
            hex2dec('1a00'), hex2dec('1a01'), [ hex2dec('6000'), 1, 1, 1;
                                                hex2dec('6000'), 2, 1, 1;
                                                hex2dec('6000'), 3, 1, 1;
                                                0              , 0, 4, 0;
                                                hex2dec('6000'), 8, 1, 1;
                                                hex2dec('6000'), 9, 1, 1;
                                                hex2dec('6000'),10, 1, 1;
                                                hex2dec('6000'),11, 1, 1;
                                                0              , 0, 1, 0;
                                                hex2dec('6000'),13, 1, 1;
                                                hex2dec('1c32'),32, 1, 0;
                                                0              , 0, 1, 0;
                                                hex2dec('1800'), 9, 1, 0;
                                                hex2dec('6000'),17,32, 1;
                                                hex2dec('6000'),18,32, 1];
            hex2dec('1a01'), hex2dec('1a00'), [ hex2dec('6000'), 1, 1, 1;
                                                hex2dec('6000'), 2, 1, 1;
                                                hex2dec('6000'), 3, 1, 1;
                                                0              , 0, 4, 0;
                                                hex2dec('6000'), 8, 1, 1;
                                                hex2dec('6000'), 9, 1, 1;
                                                hex2dec('6000'),10, 1, 1;
                                                hex2dec('6000'),11, 1, 1;
                                                0              , 0, 1, 0;
                                                hex2dec('6000'),13, 1, 1;
                                                hex2dec('1c32'),32, 1, 0;
                                                0              , 0, 1, 0;
                                                hex2dec('1801'), 9, 1, 0;
                                                hex2dec('6000'),17,16, 1;
                                                hex2dec('6000'),18,16, 1];
            hex2dec('1a02'), hex2dec('1a03'), [ hex2dec('6000'),20,32, 1];
            hex2dec('1a03'), hex2dec('1a02'), [ hex2dec('6000'),19,32, 1];
            hex2dec('1a04'), hex2dec('1a05'), [ hex2dec('6000'),22,64, 1];
            hex2dec('1a05'), hex2dec('1a04'), [ hex2dec('6000'),22,32, 1];

            % EL5152
            hex2dec('1600'), hex2dec('1601'), [ 0              , 0, 1, 0;
                                                0              , 0, 1, 0;
                                                hex2dec('7000'), 3, 1, 1;
                                                0              , 0, 1, 0;
                                                0              , 0, 4, 0;
                                                0              , 0, 8, 0;
                                                hex2dec('7000'),17,32, 1];
            hex2dec('1601'), hex2dec('1600'), [ 0              , 0, 1, 0;
                                                0              , 0, 1, 0;
                                                hex2dec('7000'), 3, 1, 1;
                                                0              , 0, 1, 0;
                                                0              , 0, 4, 0;
                                                0              , 0, 8, 0;
                                                hex2dec('7000'),17,16, 1];
            hex2dec('1602'), hex2dec('1603'), [ 0              , 0, 1, 0;
                                                0              , 0, 1, 0;
                                                hex2dec('7010'), 3, 1, 1;
                                                0              , 0, 1, 0;
                                                0              , 0, 4, 0;
                                                0              , 0, 8, 0;
                                                hex2dec('7010'),17,32, 1];
            hex2dec('1603'), hex2dec('1602'), [ 0              , 0, 1, 0;
                                                0              , 0, 1, 0;
                                                hex2dec('7010'), 3, 1, 1;
                                                0              , 0, 1, 0;
                                                0              , 0, 4, 0;
                                                0              , 0, 8, 0;
                                                hex2dec('7010'),17,16, 1];
            hex2dec('1a00'), hex2dec('1a01'), [ 0              , 0, 2, 0;
                                                hex2dec('6000'), 3, 1, 1;
                                                0              , 0, 4, 0;
                                                hex2dec('6000'), 8, 1, 1;
                                                hex2dec('6000'), 9, 1, 1;
                                                hex2dec('6000'),10, 1, 1;
                                                0              , 0, 3, 0;
                                                hex2dec('1c32'),32, 1, 0;
                                                0              , 0, 1, 0;
                                                hex2dec('1800'), 9, 1, 0;
                                                hex2dec('6000'),17,32, 1];
            hex2dec('1a01'), hex2dec('1a00'), [ 0              , 0, 2, 0;
                                                hex2dec('6000'), 3, 1, 1;
                                                0              , 0, 4, 0;
                                                hex2dec('6000'), 8, 1, 1;
                                                hex2dec('6000'), 9, 1, 1;
                                                hex2dec('6000'),10, 1, 1;
                                                0              , 0, 3, 0;
                                                hex2dec('1c32'),32, 1, 0;
                                                0              , 0, 1, 0;
                                                hex2dec('1801'), 9, 1, 0;
                                                hex2dec('6000'),17,16, 1];
            hex2dec('1a02'), hex2dec('1a03'), [ hex2dec('6000'),20,32, 1];
            hex2dec('1a03'), hex2dec('1a02'), [ hex2dec('6000'),19,32, 1];
            hex2dec('1a04'), hex2dec('1a05'), [ 0              , 0, 2, 0;
                                                hex2dec('6010'), 3, 1, 1;
                                                0              , 0, 4, 0;
                                                hex2dec('6010'), 8, 1, 1;
                                                hex2dec('6010'), 9, 1, 1;
                                                hex2dec('6010'),10, 1, 1;
                                                0              , 0, 3, 0;
                                                hex2dec('1c32'),32, 1, 0;
                                                0              , 0, 1, 0;
                                                hex2dec('1804'), 9, 1, 0;
                                                hex2dec('6010'),17,32, 1];
            hex2dec('1a05'), hex2dec('1a04'), [ 0              , 0, 2, 0;
                                                hex2dec('6010'), 3, 1, 1;
                                                0              , 0, 4, 0;
                                                hex2dec('6010'), 8, 1, 1;
                                                hex2dec('6010'), 9, 1, 1;
                                                hex2dec('6010'),10, 1, 1;
                                                0              , 0, 3, 0;
                                                hex2dec('1c32'),32, 1, 0;
                                                0              , 0, 1, 0;
                                                hex2dec('1805'), 9, 1, 0;
                                                hex2dec('6010'),17,16, 1];
            hex2dec('1a06'), hex2dec('1a07'), [ hex2dec('6010'),20,32, 1];
            hex2dec('1a07'), hex2dec('1a06'), [ hex2dec('6010'),19,32, 1];
        };

        dc = [0,0,0,0,0,0,0,0,0,0;      % FreeRun
              0,0,1,0,0,0,0,1,0,0;      % DC-Synchron
              0,0,1,0,0,1,0,1,0,0];     % DC-Synchron (input based)
    end

    properties (Constant)
        %   Model   ProductCode                      Rx    Tx
        models = {...
            'EL5151',       hex2dec('141f3052'), [], 1:2,  3:8, hex2dec('320');
            'EL5151-0080',  hex2dec('141f3052'), [], 1:2,  3:8, hex2dec('320');
            'EL5152',       hex2dec('14203052'), [],9:12,13:20, hex2dec('720');
        };
    end
end
