classdef ep2xxx < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods
        %====================================================================
        function obj = ep2xxx(id)
            if nargin > 0
                obj.slave = obj.find(id);
            end
        end

        %====================================================================
        function rv = configure(obj,vector)

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = obj.slave{1};
            rv.SlaveConfig.product = obj.slave{2};
            rv.function = obj.slave{4};

            rv.SlaveConfig.sm = arrayfun(...
                @(i) {obj.slave{5}(i,3),obj.slave{5}(i,2),obj.sm{obj.slave{5}(i,1)}}, ...
                1:size(obj.slave{5},1), 'UniformOutput', false);
                      
            rv.PortConfig.output = obj.configurePorts('I',...
                                obj.findPdoEntries(rv.SlaveConfig.sm,1,obj.slave{6}),...
                                uint(1),vector);
            rv.PortConfig.input  = obj.configurePorts('O',...
                                obj.findPdoEntries(rv.SlaveConfig.sm,0,obj.slave{6}),...
                                uint(1),vector);
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function test(p)
            ei = EtherCATInfo(fullfile(p,'Beckhoff EP2xxx.xml'));
            for i = 1:size(ep2xxx.models,1)
                fprintf('Testing %s\n', ep2xxx.models{i,1});
                slave = ei.getSlave(ep2xxx.models{i,2},...
                        'revision', ep2xxx.models{i,3});

                rv = ep2xxx(ep2xxx.models{i,1}).configure(i&1);
                slave.testConfig(rv.SlaveConfig,rv.PortConfig);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant)

                % PdoIdx         PdoEntryIdx    SubIdx  BitLen
        sm = {{{hex2dec('1600'), [hex2dec('7000'), 1, 1]},...       %1
               {hex2dec('1601'), [hex2dec('7010'), 1, 1]},... 
               {hex2dec('1602'), [hex2dec('7020'), 1, 1]},...
               {hex2dec('1603'), [hex2dec('7030'), 1, 1]},...
               {hex2dec('1604'), [hex2dec('7040'), 1, 1]},...
               {hex2dec('1605'), [hex2dec('7050'), 1, 1]},...
               {hex2dec('1606'), [hex2dec('7060'), 1, 1]},...
               {hex2dec('1607'), [hex2dec('7070'), 1, 1]}},...
              {{hex2dec('1608'), [hex2dec('7080'), 1, 1]},...       %2
               {hex2dec('1609'), [hex2dec('7090'), 1, 1]},...
               {hex2dec('160a'), [hex2dec('70a0'), 1, 1]},...
               {hex2dec('160b'), [hex2dec('70b0'), 1, 1]},...
               {hex2dec('160c'), [hex2dec('70c0'), 1, 1]},...
               {hex2dec('160d'), [hex2dec('70d0'), 1, 1]},...
               {hex2dec('160e'), [hex2dec('70e0'), 1, 1]},...
               {hex2dec('160f'), [hex2dec('70f0'), 1, 1]}},...
              {{hex2dec('1a00'), [hex2dec('6000'), 1, 1]},...       %3
               {hex2dec('1a01'), [hex2dec('6010'), 1, 1]},...
               {hex2dec('1a02'), [hex2dec('6020'), 1, 1]},...
               {hex2dec('1a03'), [hex2dec('6030'), 1, 1]},...
               {hex2dec('1a04'), [hex2dec('6040'), 1, 1]},...
               {hex2dec('1a05'), [hex2dec('6050'), 1, 1]},...
               {hex2dec('1a06'), [hex2dec('6060'), 1, 1]},...
               {hex2dec('1a07'), [hex2dec('6070'), 1, 1]}},...
              {{hex2dec('1a08'), [hex2dec('6080'), 1, 1]},...       %4
               {hex2dec('1a09'), [hex2dec('6090'), 1, 1]},...
               {hex2dec('1a0a'), [hex2dec('60a0'), 1, 1]},...
               {hex2dec('1a0b'), [hex2dec('60b0'), 1, 1]},...
               {hex2dec('1a0c'), [hex2dec('60c0'), 1, 1]},...
               {hex2dec('1a0d'), [hex2dec('60d0'), 1, 1]},...
               {hex2dec('1a0e'), [hex2dec('60e0'), 1, 1]},...
               {hex2dec('1a0f'), [hex2dec('60f0'), 1, 1]}},...
              {{hex2dec('1604'), [              0, 0, 4;            %5
                                  hex2dec('7040'), 1, 1]},...
               {hex2dec('1605'), [hex2dec('7050'), 1, 1]},...
               {hex2dec('1606'), [hex2dec('7060'), 1, 1]},...
               {hex2dec('1607'), [hex2dec('7070'), 1, 1]}},...
              {{hex2dec('1a00'), [hex2dec('6000'), 1, 1]},...       %6
               {hex2dec('1a01'), [hex2dec('6010'), 1, 1]},...
               {hex2dec('1a02'), [hex2dec('6020'), 1, 1]},...
               {hex2dec('1a03'), [hex2dec('6030'), 1, 1;     
                                                0, 0, 4]}},...
              {{hex2dec('1600'), [hex2dec('7000'), 1, 1;            %7
                                  hex2dec('7000'), 2, 1;     
                                  hex2dec('7000'), 3, 1;     
                                  hex2dec('7000'), 4, 1]},...
               {hex2dec('1601'), [hex2dec('7010'), 1, 1;            %7
                                  hex2dec('7010'), 2, 1;     
                                  hex2dec('7010'), 3, 1;     
                                  hex2dec('7010'), 4, 1]}},...
              {{hex2dec('1600'), [hex2dec('7000'), 1, 1]},...       %8
               {hex2dec('1601'), [hex2dec('7010'), 1, 1]},...
               {hex2dec('1602'), [hex2dec('7020'), 1, 1]},...
               {hex2dec('1603'), [hex2dec('7030'), 1, 1]}},...
              {{hex2dec('1600'), [hex2dec('7000'), 1, 1;            %9
                                  hex2dec('7000'), 2, 1;     
                                  hex2dec('7000'), 3, 1;     
                                  hex2dec('7000'), 4, 1;     
                                  hex2dec('7000'), 5, 1;     
                                  hex2dec('7000'), 6, 1;     
                                  hex2dec('7000'), 7, 1;     
                                  hex2dec('7000'), 8, 1;     
                                                0, 0, 8]},...
               {hex2dec('1601'), [hex2dec('f700'), 1, 1;
                                  hex2dec('f700'), 2, 1;
                                                0, 0,14]}},...
              {{hex2dec('1a00'), [hex2dec('6000'), 1, 1;            %10
                                  hex2dec('6000'), 2, 1;
                                  hex2dec('6000'), 3, 1;
                                  hex2dec('6000'), 4, 1;
                                  hex2dec('6000'), 5, 1;
                                  hex2dec('6000'), 6, 1;
                                  hex2dec('6000'), 7, 1;
                                  hex2dec('6000'), 8, 1;
                                                0, 0, 5;
                                  hex2dec('1c32'),32, 1;
                                                0, 0, 2]},...
               {hex2dec('1a01'), [hex2dec('6001'), 1, 1;
                                  hex2dec('6001'), 2, 1;
                                  hex2dec('6001'), 3, 1;
                                  hex2dec('6001'), 4, 1;
                                  hex2dec('6001'), 5, 1;
                                  hex2dec('6001'), 6, 1;
                                  hex2dec('6001'), 7, 1;
                                  hex2dec('6001'), 8, 1;
                                                0, 0, 8]},...
               {hex2dec('1a02'), [hex2dec('f600'), 1, 1;
                                  hex2dec('f600'), 2, 1;
                                  hex2dec('f600'), 3, 1;
                                                0, 0,10;
                                  hex2dec('1c32'),32, 1;
                                                0, 0, 1;
                                  hex2dec('1800'), 9, 1]}},...
        };

        %   Model,        ProductCode,         Revision,
        %       Description, [PdoPdoDefinition
        models = {
           'EP2001-1000', hex2dec('07d14052'), hex2dec('001003e8'), ...
                '8 Ch DigOut',  [7, 0, 0], 8; %      SmGrp,Dir,SmIdx
           'EP2008-0001', hex2dec('07d84052'), hex2dec('00100001'), ...
                '8 Ch DigOut',  [1, 0, 0], 8;
           'EP2008-0002', hex2dec('07d84052'), hex2dec('00100002'), ...
                '8 Ch DigOut',  [1, 0, 0], 8;
           'EP2008-0022', hex2dec('07d84052'), hex2dec('00100016'), ...
                '8 Ch DigOut',  [1, 0, 0], 8;
           'EP2028-0001', hex2dec('07ec4052'), hex2dec('00100001'), ...
                '8 Ch DigOut',  [1, 0, 0], 8;
           'EP2028-0002', hex2dec('07ec4052'), hex2dec('00100002'), ...
                '8 Ch DigOut',  [1, 0, 0], 8;
           'EP2028-0032', hex2dec('07ec4052'), hex2dec('00110020'), ...
                '8 Ch DigOut',  [1, 0, 0], 8;
           'EP2308-0000', hex2dec('09044052'), hex2dec('00100000'), ...
                '4 Ch DigIO',   [5, 0, 0; 6, 1, 1], 4;
           'EP2308-0001', hex2dec('09044052'), hex2dec('00100001'), ...
                '4 Ch DigIO',   [5, 0, 0; 6, 1, 1], 4;
           'EP2308-0002', hex2dec('09044052'), hex2dec('00100002'), ...
                '4 Ch DigIO',   [5, 0, 0; 6, 1, 1], 4;
           'EP2316-0003', hex2dec('090c4052'), hex2dec('00100003'), ...
                '8 Ch DigIO',   [9, 0, 2; 10, 1, 3], 8;
           'EP2316-0008', hex2dec('090c4052'), hex2dec('00100008'), ...
                '8 Ch DigIO',   [9, 0, 2; 10, 1, 3], 8;
           'EP2318-0001', hex2dec('090e4052'), hex2dec('00100001'), ...
                '4 Ch DigIO',   [5, 0, 0; 6, 1, 1], 4;
           'EP2318-0002', hex2dec('090e4052'), hex2dec('00100002'), ...
                '4 Ch DigIO',   [5, 0, 0; 6, 1, 1], 4;
           'EP2328-0001', hex2dec('09184052'), hex2dec('00100001'), ...
                '4 Ch DigIO',   [5, 0, 0; 6, 1, 1], 4;
           'EP2328-0002', hex2dec('09184052'), hex2dec('00100002'), ...
                '4 Ch DigIO',   [5, 0, 0; 6, 1, 1], 4;
           'EP2338-0001', hex2dec('09224052'), hex2dec('00100001'), ...
                '8 Ch DigIO',   [1, 0, 0; 3, 1, 1], 8;
           'EP2338-0002', hex2dec('09224052'), hex2dec('00100002'), ...
                '8 Ch DigIO',   [1, 0, 0; 3, 1, 1], 8;
           'EP2338-1001', hex2dec('09224052'), hex2dec('001003e9'), ...
                '8 Ch DigIO',   [1, 0, 0; 3, 1, 1], 8;
           'EP2338-1002', hex2dec('09224052'), hex2dec('001003ea'), ...
                '8 Ch DigIO',   [1, 0, 0; 3, 1, 1], 8;
           'EP2339-0021', hex2dec('09234052'), hex2dec('00100015'), ...
                 '16 Ch DigIO', [1, 0, 0; 2, 0, 1; 3, 1, 2; 4, 1, 3], 16;
           'EP2339-0022', hex2dec('09234052'), hex2dec('00100016'), ...
                 '16 Ch DigIO', [1, 0, 0; 2, 0, 1; 3, 1, 2; 4, 1, 3], 16;
           'EP2349-0021', hex2dec('092d4052'), hex2dec('00100015'), ...
                 '16 Ch DigIO', [1, 0, 0; 2, 0, 1; 3, 1, 2; 4, 1, 3], 16;
           'EP2349-0022', hex2dec('092d4052'), hex2dec('00100016'), ...
                 '16 Ch DigIO', [1, 0, 0; 2, 0, 1; 3, 1, 2; 4, 1, 3], 16;
           'EP2624', hex2dec('0a404052'), hex2dec('00100002'), ...
                '4Ch Relay',    [8, 0, 0], 4;
           'EP2624-0002', hex2dec('0a404052'), hex2dec('00110002'), ...
                '4Ch Relay',    [8, 0, 0], 4;
           'EP2809-0021', hex2dec('0af94052'), hex2dec('00100015'), ...
                '16Ch DigOut',  [1, 0, 0; 2, 0, 1], 16;
           'EP2809-0022', hex2dec('0af94052'), hex2dec('00100016'), ...
                '16Ch DigOut',  [1, 0, 0; 2, 0, 1], 16;
        };
    end
end
