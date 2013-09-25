classdef el2xxx < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function rv = hasDiag(model)
            slave = el2xxx.findSlave(model, el2xxx.models);
            rv = slave{6};
        end

        %====================================================================
        function rv = configure(model,vector,status)
            slave = el2xxx.findSlave(model, el2xxx.models);

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = model;
            rv.SlaveConfig.product  = slave{2};

            pdo_count = slave{4};

            pdo_idx = [slave{5} + (0:pdo_count-1)];

            status = status && slave{6};
            if status
                pdo_idx = [pdo_idx, slave{6} + (0:pdo_count-1)];
            end

            pdo_list = el2xxx.pdo;
            pdo = pdo_list(pdo_idx,:);

            sm_id = unique(pdo(:,1))';
            rv.SlaveConfig.sm = repmat({{0,0,{}}}, size(sm_id));

            for i = 1:numel(sm_id)
                p = arrayfun(@(i) {pdo(i,2), [pdo(i,[3,4]),1]},...
                        find(pdo(:,1) == sm_id(i))', ...
                        'UniformOutput', false);
                dir = status && (pdo_list(slave{6},1) == sm_id(i));
                dir = double(dir);
                rv.SlaveConfig.sm{i} = {sm_id(i), dir, p};
            end

            rv.PortConfig.input  = el2xxx.configurePorts('O',...
                                el2xxx.findPdoEntries(rv.SlaveConfig.sm,0),...
                                uint(1),vector);

            if status
                rv.PortConfig.output = el2xxx.configurePorts('I',...
                        el2xxx.findPdoEntries(rv.SlaveConfig.sm,1),...
                        uint(1),vector);
            else
                rv.PortConfig.output = [];
            end
        end

        %====================================================================
        function test(p)
            ei = EtherCATInfo(fullfile(p,'Beckhoff EL2xxx.xml'));
            for i = 1:size(el2xxx.models,1)
                fprintf('Testing %s\n', el2xxx.models{i,1});
                rv = el2xxx.configure(el2xxx.models{i,1},i&1,0);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);

                rv = el2xxx.configure(el2xxx.models{i,1},i&1,1);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant)

        %      Sm   PdoIdx          EntryIdx      SubIdx
        pdo = [ 0, hex2dec('1600'), hex2dec('7000'), 1;    %   1
                0, hex2dec('1601'), hex2dec('7010'), 1;    %   2
                0, hex2dec('1602'), hex2dec('7020'), 1;    %   3
                0, hex2dec('1603'), hex2dec('7030'), 1;    %   4
                0, hex2dec('1604'), hex2dec('7040'), 1;    %   5
                0, hex2dec('1605'), hex2dec('7050'), 1;    %   6
                0, hex2dec('1606'), hex2dec('7060'), 1;    %   7
                0, hex2dec('1607'), hex2dec('7070'), 1;    %   8
                1, hex2dec('1608'), hex2dec('7080'), 1;    %   9
                1, hex2dec('1609'), hex2dec('7090'), 1;    %  10
                1, hex2dec('160a'), hex2dec('70a0'), 1;    %  11
                1, hex2dec('160b'), hex2dec('70b0'), 1;    %  12
                1, hex2dec('160c'), hex2dec('70c0'), 1;    %  13
                1, hex2dec('160d'), hex2dec('70d0'), 1;    %  14
                1, hex2dec('160e'), hex2dec('70e0'), 1;    %  15
                1, hex2dec('160f'), hex2dec('70f0'), 1;    %  16

                1, hex2dec('1a00'), hex2dec('6000'), 1;    %  17
                1, hex2dec('1a01'), hex2dec('6010'), 1;    %  18
                1, hex2dec('1a02'), hex2dec('6020'), 1;    %  19
                1, hex2dec('1a03'), hex2dec('6030'), 1;    %  20

                0, hex2dec('1600'), hex2dec('3001'), 1;    %  21
                0, hex2dec('1601'), hex2dec('3001'), 2;    %  22
                0, hex2dec('1602'), hex2dec('3001'), 3;    %  23
                0, hex2dec('1603'), hex2dec('3001'), 4;    %  24

                1, hex2dec('1a00'), hex2dec('3101'), 1;    %  25
                1, hex2dec('1a01'), hex2dec('3101'), 2];   %  26

        %   Model       ProductCode          Revision          N, Rx, Tx
        models = {...
            'EL2002', hex2dec('07d23052'), hex2dec('00100000'), 2, 1,  0;
            'EL2004', hex2dec('07d43052'), hex2dec('00100000'), 4, 1,  0;
            'EL2008', hex2dec('07d83052'), hex2dec('00100000'), 8, 1,  0;
            'EL2022', hex2dec('07e63052'), hex2dec('00100000'), 2, 1,  0;
            'EL2024', hex2dec('07e83052'), hex2dec('00100000'), 4, 1,  0;
            'EL2032', hex2dec('07f03052'), hex2dec('00100000'), 2, 1, 17;
            'EL2034', hex2dec('07f23052'), hex2dec('00100000'), 4, 1, 17;
            'EL2042', hex2dec('07fa3052'), hex2dec('00100000'), 2, 1,  0;
            'EL2084', hex2dec('08243052'), hex2dec('00100000'), 4, 1,  0;
            'EL2088', hex2dec('08283052'), hex2dec('00100000'), 8, 1,  0;
            'EL2124', hex2dec('084c3052'), hex2dec('00100000'), 4, 1,  0;
            'EL2202', hex2dec('089a3052'), hex2dec('00100000'), 2, 1,  0;
            'EL2602', hex2dec('0a2a3052'), hex2dec('00100000'), 2, 1,  0;
            'EL2612', hex2dec('0a343052'), hex2dec('00100000'), 2, 1,  0;
            'EL2622', hex2dec('0a3e3052'), hex2dec('00100000'), 2, 1,  0;
            'EL2624', hex2dec('0a403052'), hex2dec('00100000'), 4, 1,  0;
            'EL2712', hex2dec('0a983052'), hex2dec('00100000'), 2, 1,  0;
            'EL2722', hex2dec('0aa23052'), hex2dec('00100000'), 2, 1,  0;
            'EL2732', hex2dec('0aac3052'), hex2dec('00100000'), 2, 1,  0;
            'EL2798', hex2dec('0aee3052'), hex2dec('00100000'), 8, 1,  0;
            'EL2808', hex2dec('0af83052'), hex2dec('00100000'), 8, 1,  0;
            'EL2809', hex2dec('0af93052'), hex2dec('00100000'),16, 1,  0;
            'EL2004-0000-0000', ...
                      hex2dec('07d43052'), hex2dec('00000000'), 4, 21, 0;
            'EL2032-0000-0000', ...
                      hex2dec('07f03052'), hex2dec('00000000'), 2, 21,25;
        };

    end
end
