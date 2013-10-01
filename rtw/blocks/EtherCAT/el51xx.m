classdef el51xx < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function rv = getExcludeList(model, checked)
            slave = EtherCATSlave.findSlave(model,el51xx.models);
            pdo_list = el51xx.pdo;

            % row list of all pdo's
            rows = [slave{[4,5]}];

            % Remove pdo's that do not belong to the slave
            checked = checked(ismember(checked, [pdo_list{rows,1}]));

            % Select 
            rows = rows(ismember([pdo_list{rows,1}], checked));

            % return column 2 of pdo_list, the exclude list
            rv = [pdo_list{rows,2}];
        end

        %====================================================================
        function rv = getSDO(model)
            slave = EtherCATSlave.findSlave(model,el51xx.models);
            rv = slave{6};
        end

        %====================================================================
        function rv = pdoVisible(model)
            slave = EtherCATSlave.findSlave(model,el51xx.models);
            pdo_list = el51xx.pdo;

            rv = [pdo_list{[slave{[4,5]}],1}];
        end 

        %====================================================================
        function rv = configure(model,mapped_pdo,sdo_config,dc_config)
            slave = EtherCATSlave.findSlave(model,el51xx.models);

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = model;
            rv.SlaveConfig.product  = slave{2};

            % Get a list of pdo's for the selected slave
            pdo_list = el51xx.pdo;
            selected = ismember(1:size(pdo_list,1), [slave{4},slave{5}]) ...
                & ismember([pdo_list{:,1}], mapped_pdo);

            % Reduce this list to the ones selected, making sure that
            % excluded pdo's are not mapped
            exclude = [];
            for i = 1:numel(selected)
                selected(i) = selected(i) & ~ismember(pdo_list{i,1}, exclude);
                if selected(i)
                    exclude = [exclude,pdo_list{i,2}];
                end
            end

            % Configure SM2 and SM3
            i = 1:numel(selected);
            rx = i(ismember(1:numel(selected), slave{4}) & selected);
            tx = i(ismember(1:numel(selected), slave{5}) & selected);
            rv.SlaveConfig.sm = ...
                {{2,0, arrayfun(@(x) {pdo_list{x,1}, ...
                                      pdo_list{x,3}(:,1:3)},...
                                rx, 'UniformOutput', false)}, ...
                 {3,1, arrayfun(@(x) {pdo_list{x,1}, ...
                                      pdo_list{x,3}(:,1:3)},...
                                tx, 'UniformOutput', false)}};

            % Configure input port. The algorithm below will group all boolean
            % signals to one port. All other entries get a separate port
            inputs = arrayfun(@(i) arrayfun(@(j) {i-1, ...
                                                  pdo_list{rx(i),3}(1,3), ...
                                                  pdo_list{rx(i),4}{j,1}',...
                                                  pdo_list{rx(i),4}{j,2}},...
                                            1:size(pdo_list{rx(i),4},1),...
                                            'UniformOutput', false), ...
                              1:numel(rx), 'UniformOutput', false);
            if ~isempty(inputs)
                inputs = horzcat(inputs{:});
            end

            rv.PortConfig.input = ...
                cellfun(@(i) struct('pdo',horzcat(repmat([0,i{1}], ...
                                                         size(i{3})), ...
                                                  i{3}, zeros(size(i{3}))), ...
                                    'pdo_data_type', uint(i{2}), ...
                                    'portname', i{4}), ...
                         inputs);

            % Configure output port. The algorithm below will group all boolean
            % signals to one port. All other entries get a separate port
            outputs = arrayfun(@(i) arrayfun(@(j) {i-1, ...
                                                   pdo_list{tx(i),3}(1,3), ...
                                                   pdo_list{tx(i),4}{j,1}',...
                                                   pdo_list{tx(i),4}{j,2}},...
                                            1:size(pdo_list{tx(i),4},1),...
                                            'UniformOutput', false), ...
                              1:numel(tx), 'UniformOutput', false);
            if ~isempty(outputs)
                outputs = horzcat(outputs{:});
            end

            rv.PortConfig.output = ...
                cellfun(@(i) struct('pdo',horzcat(repmat([1,i{1}], ...
                                                         size(i{3})), ...
                                                  i{3}, zeros(size(i{3}))), ...
                                    'pdo_data_type', uint(i{2}), ...
                                    'portname', i{4}), ...
                         outputs);

            % Distributed clocks
            if dc_config(1) == 4
                rv.PortConfig.dc = dc_config(2:11);
            else
                dc = el51xx.dc;
                rv.PortConfig.dc = dc(dc_config(1),:);
                rv.PortConfig.dc(1) = slave{7}; % Set AssignActivate
            end

            % CoE Configuration
            sdo = el51xx.sdo;
            rv.SlaveConfig.sdo = ...
                num2cell(horzcat(sdo(slave{6},:), sdo_config(slave{6})'));
        end

        %====================================================================
        function test(p)
            ei = EtherCATInfo(fullfile(p,'Beckhoff EL5xxx.xml'));
            pdo = el51xx.pdo;
            for i = 1:size(el51xx.models,1)
                fprintf('Testing %s\n', el51xx.models{i,1});
                l = [el51xx.models{1,4},el51xx.models{1,5}];

                pdoIdx = cell2mat(pdo(l,1))';
                rv = el51xx.configure(el51xx.models{1,1}, pdoIdx, 1:50,2);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);

                pdoIdx = cellfun(@(x) x(1), pdo(l,2))';
                rv = el51xx.configure(el51xx.models{1,1}, pdoIdx, 1:50,2);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant) %, Access = private)

        pdo = {
                % EL5101,EL5101-1006 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
                hex2dec('1601'), hex2dec({'1600','1602','1603'})', ...
                   [ hex2dec('7000'), 1, 8;
                     0              , 0, 8;
                     hex2dec('7000'), 2,16],...
                   { 0, 'Ctrl'; 2, 'Value' };
                hex2dec('1602'), hex2dec({'1600','1601','1603'})', ...
                   [ hex2dec('7010'), 1, 1;
                     hex2dec('7010'), 2, 1;
                     hex2dec('7010'), 3, 1;
                     hex2dec('7010'), 4, 1;
                     0              , 0, 4;
                     0              , 0, 8;
                     hex2dec('7010'),17,16],...
                   { 0:3, 'bool[4]'; 6, 'Value' };
                hex2dec('1603'), hex2dec({'1600','1601','1602'})', ...
                   [ hex2dec('7010'), 1, 1;
                     hex2dec('7010'), 2, 1;
                     hex2dec('7010'), 3, 1;
                     hex2dec('7010'), 4, 1;
                     0              , 0, 4;
                     0              , 0, 8;
                     hex2dec('7010'),17,32],...
                   { 0:3, 'bool[4]'; 6, 'Value' };
                hex2dec('1a01'), hex2dec({'1a00',       '1a03','1a04',...
                                          '1a05','1a06','1a07','1a08'})',...
                   [ hex2dec('6000'), 1, 8;
                     0              , 0, 8;
                     hex2dec('6000'), 2,16;
                     hex2dec('6000'), 3,16],...
                   { 0, 'Status'; 2, 'Counter'; 3, 'Latch' };
                hex2dec('1a02'), hex2dec({              '1a03','1a04',...
                                          '1a05','1a06','1a07','1a08'})',...
                   [ hex2dec('6000'), 4,32;
                     hex2dec('6000'), 5,16;
                     hex2dec('6000'), 6,16],...
                   { 0, 'Freq'; 1, 'Period'; 2, 'Window' };
                hex2dec('1a03'), hex2dec({'1a01','1a02','1a04'})',...
                   [ hex2dec('6010'), 1, 1;
                     hex2dec('6010'), 2, 1;
                     hex2dec('6010'), 3, 1;
                     hex2dec('6010'), 4, 1;
                     hex2dec('6010'), 5, 1;
                     hex2dec('6010'), 6, 1;
                     hex2dec('6010'), 7, 1;
                     hex2dec('6010'), 8, 1;
                     hex2dec('6010'), 9, 1;
                     hex2dec('6010'),10, 1;
                     hex2dec('6010'),11, 1;
                     hex2dec('6010'),12, 1;
                     hex2dec('6010'),13, 1;
                     hex2dec('1c32'),32, 1;
                     hex2dec('1803'), 7, 1;
                     hex2dec('1803'), 9, 1;
                     hex2dec('6010'),17,16;
                     hex2dec('6010'),18,16],...
                   { 0:12, 'bool[13]'; 16, 'Counter'; 17, 'Latch' };
                hex2dec('1a04'), hex2dec({'1a01','1a02','1a03'})',...
                   [ hex2dec('6010'), 1, 1;
                     hex2dec('6010'), 2, 1;
                     hex2dec('6010'), 3, 1;
                     hex2dec('6010'), 4, 1;
                     hex2dec('6010'), 5, 1;
                     hex2dec('6010'), 6, 1;
                     hex2dec('6010'), 7, 1;
                     hex2dec('6010'), 8, 1;
                     hex2dec('6010'), 9, 1;
                     hex2dec('6010'),10, 1;
                     hex2dec('6010'),11, 1;
                     hex2dec('6010'),12, 1;
                     hex2dec('6010'),13, 1;
                     hex2dec('1c32'),32, 1;
                     hex2dec('1804'), 7, 1;
                     hex2dec('1804'), 9, 1;
                     hex2dec('6010'),17,32;
                     hex2dec('6010'),18,32],...
                   { 0:12, 'bool[13]'; 16, 'Counter'; 17, 'Latch' };
                hex2dec('1a05'), hex2dec({'1a01','1a02','1a06'})',...
                   [ hex2dec('6010'),19,32],...
                   { 0, 'Freq' };
                hex2dec('1a06'), hex2dec({'1a01','1a02','1a05'})',...
                   [ hex2dec('6010'),20,32],...
                   { 0, 'Period' };
                hex2dec('1a07'), hex2dec({'1a01','1a02','1a08'})',...
                   [ hex2dec('6010'),22,64],...
                   { 0, 'Time' };
                hex2dec('1a08'), hex2dec({'1a01','1a02','1a07'})',...
                   [ hex2dec('6010'),22,32],...
                   { 0, 'Time' };

                % EL5101-0010 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
                hex2dec('1600'), hex2dec('1601'), [ hex2dec('7000'), 1, 1;
                                                    hex2dec('7000'), 2, 1;
                                                    hex2dec('7000'), 3, 1;
                                                    hex2dec('7000'), 4, 1;
                                                    0              , 0, 4;
                                                    0              , 0, 8;
                                                    hex2dec('7000'),17,32], ...
                       {0:3, 'bool[4]'; 6, 'Value'};
                hex2dec('1601'), hex2dec('1600'), [ hex2dec('7000'), 1, 1;
                                                    hex2dec('7000'), 2, 1;
                                                    hex2dec('7000'), 3, 1;
                                                    hex2dec('7000'), 4, 1;
                                                    0              , 0, 4;
                                                    0              , 0, 8;
                                                    hex2dec('7000'),17,16], ...
                       {0:3, 'bool[4]'; 6, 'Value'};
                hex2dec('1a00'), hex2dec('1a01'), [ hex2dec('6000'), 1, 1;
                                                    hex2dec('6000'), 2, 1;
                                                    hex2dec('6000'), 3, 1;
                                                    0              , 0, 2;
                                                    hex2dec('6000'), 6, 1;
                                                    hex2dec('6000'), 7, 1;
                                                    0              , 0, 1;
                                                    hex2dec('6000'), 9, 1;
                                                    hex2dec('6000'),10, 1;
                                                    hex2dec('6000'),11, 1;
                                                    hex2dec('6000'),12, 1;
                                                    hex2dec('6000'),13, 1;
                                                    hex2dec('1c32'),32, 1;
                                                    hex2dec('1800'), 7, 1;
                                                    hex2dec('1800'), 9, 1;
                                                    hex2dec('6000'),17,32;
                                                    hex2dec('6000'),18,32], ...
                       {[0:2,4,5,7:11], 'bool[10]'; 15, 'Counter'; 16, 'Latch'};
                hex2dec('1a01'), hex2dec('1a00'), [ hex2dec('6000'), 1, 1;
                                                    hex2dec('6000'), 2, 1;
                                                    hex2dec('6000'), 3, 1;
                                                    0              , 0, 2;
                                                    hex2dec('6000'), 6, 1;
                                                    hex2dec('6000'), 7, 1;
                                                    0              , 0, 1;
                                                    hex2dec('6000'), 9, 1;
                                                    hex2dec('6000'),10, 1;
                                                    hex2dec('6000'),11, 1;
                                                    hex2dec('6000'),12, 1;
                                                    hex2dec('6000'),13, 1;
                                                    hex2dec('1c32'),32, 1;
                                                    hex2dec('1800'), 7, 1;
                                                    hex2dec('1800'), 9, 1;
                                                    hex2dec('6000'),17,16;
                                                    hex2dec('6000'),18,16], ...
                       {[0:2,4,5,7:11], 'bool[10]'; 15, 'Counter'; 16, 'Latch'};
                hex2dec('1a02'), hex2dec('1a03'), [ hex2dec('6000'),20,32], ...
                       {0, 'Period'};
                hex2dec('1a03'), hex2dec('1a02'), [ hex2dec('6000'),19,32], ...
                       {0, 'Freq'};
                hex2dec('1a04'), hex2dec('1a05'), [ hex2dec('6000'),22,64], ...
                       {0, 'Time'};
                hex2dec('1a05'), hex2dec('1a04'), [ hex2dec('6000'),22,32], ...
                       {0, 'Time'};

                % EL5151 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
                % Same as EL5101-0010, except for
                hex2dec('1a00'), hex2dec('1a01'), [ hex2dec('6000'), 1, 1;
                                                    hex2dec('6000'), 2, 1;
                                                    hex2dec('6000'), 3, 1;
                                                    0              , 0, 4;
                                                    hex2dec('6000'), 8, 1;
                                                    hex2dec('6000'), 9, 1;
                                                    hex2dec('6000'),10, 1;
                                                    hex2dec('6000'),11, 1;
                                                    0              , 0, 1;
                                                    hex2dec('6000'),13, 1;
                                                    hex2dec('1c32'),32, 1;
                                                    0              , 0, 1;
                                                    hex2dec('1800'), 9, 1;
                                                    hex2dec('6000'),17,32;
                                                    hex2dec('6000'),18,32], ...
                       {[0:2,4:7,9], 'bool[8]'; 13, 'Counter'; 14, 'Latch'};
                hex2dec('1a01'), hex2dec('1a00'), [ hex2dec('6000'), 1, 1;
                                                    hex2dec('6000'), 2, 1;
                                                    hex2dec('6000'), 3, 1;
                                                    0              , 0, 4;
                                                    hex2dec('6000'), 8, 1;
                                                    hex2dec('6000'), 9, 1;
                                                    hex2dec('6000'),10, 1;
                                                    hex2dec('6000'),11, 1;
                                                    0              , 0, 1;
                                                    hex2dec('6000'),13, 1;
                                                    hex2dec('1c32'),32, 1;
                                                    0              , 0, 1;
                                                    hex2dec('1801'), 9, 1;
                                                    hex2dec('6000'),17,16;
                                                    hex2dec('6000'),18,16], ...
                       {[0:2,4:7,9], 'bool[8]'; 13, 'Counter'; 14, 'Latch'};

                % EL5152 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
                hex2dec('1600'), hex2dec('1601'), [ 0              , 0, 1;
                                                    0              , 0, 1;
                                                    hex2dec('7000'), 3, 1;
                                                    0              , 0, 1;
                                                    0              , 0, 4;
                                                    0              , 0, 8;
                                                    hex2dec('7000'),17,32], ...
                       {2, 'Set'; 6, 'Value'};
                hex2dec('1601'), hex2dec('1600'), [ 0              , 0, 1;
                                                    0              , 0, 1;
                                                    hex2dec('7000'), 3, 1;
                                                    0              , 0, 1;
                                                    0              , 0, 4;
                                                    0              , 0, 8;
                                                    hex2dec('7000'),17,16], ...
                       {2, 'Set'; 6, 'Value'};
                hex2dec('1602'), hex2dec('1603'), [ 0              , 0, 1;
                                                    0              , 0, 1;
                                                    hex2dec('7010'), 3, 1;
                                                    0              , 0, 1;
                                                    0              , 0, 4;
                                                    0              , 0, 8;
                                                    hex2dec('7010'),17,32], ...
                       {2, 'Set'; 6, 'Value'};
                hex2dec('1603'), hex2dec('1602'), [ 0              , 0, 1;
                                                    0              , 0, 1;
                                                    hex2dec('7010'), 3, 1;
                                                    0              , 0, 1;
                                                    0              , 0, 4;
                                                    0              , 0, 8;
                                                    hex2dec('7010'),17,16], ...
                       {2, 'Set'; 6, 'Value'};
                hex2dec('1a00'), hex2dec('1a01'), [ 0              , 0, 2;
                                                    hex2dec('6000'), 3, 1;
                                                    0              , 0, 4;
                                                    hex2dec('6000'), 8, 1;
                                                    hex2dec('6000'), 9, 1;
                                                    hex2dec('6000'),10, 1;
                                                    0              , 0, 3;
                                                    hex2dec('1c32'),32, 1;
                                                    0              , 0, 1;
                                                    hex2dec('1800'), 9, 1;
                                                    hex2dec('6000'),17,32], ...
                       {[1,3,4,5], 'bool[4]'; 10, 'Counter'};
                hex2dec('1a01'), hex2dec('1a00'), [ 0              , 0, 2;
                                                    hex2dec('6000'), 3, 1;
                                                    0              , 0, 4;
                                                    hex2dec('6000'), 8, 1;
                                                    hex2dec('6000'), 9, 1;
                                                    hex2dec('6000'),10, 1;
                                                    0              , 0, 3;
                                                    hex2dec('1c32'),32, 1;
                                                    0              , 0, 1;
                                                    hex2dec('1801'), 9, 1;
                                                    hex2dec('6000'),17,16], ...
                       {[1,3,4,5], 'bool[4]'; 10, 'Counter'};
                hex2dec('1a02'), hex2dec('1a03'), [ hex2dec('6000'),20,32], ...
                       {0, 'Period'};
                hex2dec('1a03'), hex2dec('1a02'), [ hex2dec('6000'),19,32], ...
                       {0, 'Freq'};
                hex2dec('1a04'), hex2dec('1a05'), [ 0              , 0, 2;
                                                    hex2dec('6010'), 3, 1;
                                                    0              , 0, 4;
                                                    hex2dec('6010'), 8, 1;
                                                    hex2dec('6010'), 9, 1;
                                                    hex2dec('6010'),10, 1;
                                                    0              , 0, 3;
                                                    hex2dec('1c32'),32, 1;
                                                    0              , 0, 1;
                                                    hex2dec('1804'), 9, 1;
                                                    hex2dec('6010'),17,32], ...
                       {[1,3,4,5], 'bool2[4]'; 10, 'Counter2'};
                hex2dec('1a05'), hex2dec('1a04'), [ 0              , 0, 2;
                                                    hex2dec('6010'), 3, 1;
                                                    0              , 0, 4;
                                                    hex2dec('6010'), 8, 1;
                                                    hex2dec('6010'), 9, 1;
                                                    hex2dec('6010'),10, 1;
                                                    0              , 0, 3;
                                                    hex2dec('1c32'),32, 1;
                                                    0              , 0, 1;
                                                    hex2dec('1805'), 9, 1;
                                                    hex2dec('6010'),17,16], ...
                       {[1,3,4,5], 'bool2[4]'; 10, 'Counter2'};
                hex2dec('1a06'), hex2dec('1a07'), [ hex2dec('6010'),20,32], ...
                       {0, 'Period'};
                hex2dec('1a07'), hex2dec('1a06'), [ hex2dec('6010'),19,32], ...
                       {0, 'Freq'};
        };

        dc = [0,0,0,0,0,0,0,0,0,0;      % FreeRun
              0,0,1,0,0,0,0,1,0,0;      % DC-Synchron
              0,0,1,0,0,1,0,1,0,0];     % DC-Synchron (input based)

        % CoE Definition
        %% EL5101
        %% 8000
        %    '1 1 Enable register reload'
        %    '2 1 Enable index reset'
        %    '3 1 Enable FWD count'
        %    '4 1 Enable pos. gate'
        %    '5 1 Enable neg. gate'
        %% 8001
        %    '1 16 Frequency window'
        %    '2 16 Counter reload value'
        %% 8010
        %    '1 1 Enable C reset'
        %    '2 1 Enable extern reset'
        %    '3 1 Enable up/down counter'
        %    '4 2 Gate polarity'
        %    '8 1 Disable filter'
        %    '10 1 Enable micro increments'
        %    '11 1 Open circuit detection A'
        %    '12 1 Open circuit detection B'
        %    '13 1 Open circuit detection C'
        %    '14 1 Reversion of rotation'
        %    '16 1 Extern reset polarity'
        %    '17 16 Frequency window'
        %    '19 16 Frequency scaling'
        %    '20 16 Period scaling'
        %    '21 16 Frequency resolution'
        %    '22 16 Period resolution'
        %    '23 16 Frequency wait time'
        %% EL5101-0010
        %% 8000
        %    '1 1 Enable C reset'
        %    '2 1 Enable extern reset'
        %    '4 2 Gate polarity'
        %    '11 1 Open circuit detection A'
        %    '12 1 Open circuit detection B'
        %    '13 1 Open circuit detection C'
        %    '14 1 Reversion of rotation'
        %    '16 1 Extern reset polarity'
        %    '17 16 Frequency window'
        %    '19 16 Frequency scaling'
        %    '20 16 Period scaling'
        %    '21 16 Frequency resolution'
        %    '22 16 Period resolution'
        %    '23 16 Frequency wait time'
        %% EL5101-1006
        %% 8000
        %    '1 1 Enable C reset'
        %    '2 1 Enable extern reset'
        %    '4 2 Gate polarity'
        %    '11 1 Open circuit detection A'
        %    '12 1 Open circuit detection B'
        %    '13 1 Open circuit detection C'
        %    '14 1 Reversion of rotation'
        %    '16 1 Extern reset polarity'
        %    '17 16 Frequency window'
        %    '19 16 Frequency scaling'
        %    '20 16 Period scaling'
        %    '21 16 Frequency resolution'
        %    '22 16 Period resolution'
        %    '23 16 Frequency wait time'
        %% EL5151
        %% 8000
        %    '1 1 Enable C reset'
        %    '2 1 Enable extern reset'
        %    '4 2 Gate polarity'
        %    '11 1 Open circuit detection A'
        %    '12 1 Open circuit detection B'
        %    '13 1 Open circuit detection C'
        %    '14 1 Reversion of rotation'
        %    '16 1 Extern reset polarity'
        %    '17 16 Frequency window'
        %    '19 16 Frequency scaling'
        %    '20 16 Period scaling'
        %    '21 16 Frequency resolution'
        %    '22 16 Period resolution'
        %    '23 16 Frequency wait time'
        %% EL5151-0080
        %% 8000
        %    '1 1 Enable C reset'
        %    '2 1 Enable extern reset'
        %    '4 2 Gate polarity'
        %    '11 1 Open circuit detection A'
        %    '12 1 Open circuit detection B'
        %    '13 1 Open circuit detection C'
        %    '14 1 Reversion of rotation'
        %    '16 1 Extern reset polarity'
        %    '17 16 Frequency window'
        %    '19 16 Frequency scaling'
        %    '20 16 Period scaling'
        %    '21 16 Frequency resolution'
        %    '22 16 Period resolution'
        %    '23 16 Frequency wait time'
        %% EL5152
        %% 8000,8010
        %    '1 1 Enable C reset'
        %    '2 1 Enable extern reset'
        %    '4 2 Gate polarity'
        %    '11 1 Open circuit detection A'
        %    '12 1 Open circuit detection B'
        %    '13 1 Open circuit detection C'
        %    '14 1 Reversion of rotation'
        %    '16 1 Extern reset polarity'
        %    '17 16 Frequency window'
        %    '19 16 Frequency scaling'
        %    '20 16 Period scaling'
        %    '21 16 Frequency resolution'
        %    '22 16 Period resolution'
        %    '23 16 Frequency wait time'
        sdo = [ 
                % EL5101
                hex2dec('8000'),  1,  8; % 1    ' 1 1 Enable register reload'
                hex2dec('8000'),  2,  8; % 2    ' 2 1 Enable index reset'
                hex2dec('8000'),  3,  8; % 3    ' 3 1 Enable FWD count'
                hex2dec('8000'),  4,  8; % 4    ' 4 1 Enable pos. gate'
                hex2dec('8000'),  5,  8; % 5    ' 5 1 Enable neg. gate'
                hex2dec('8000'),  1,  8; % 6    ' 1 1 Enable C reset'
                hex2dec('8000'),  2,  8; % 7    ' 2 1 Enable extern reset'
                hex2dec('8000'),  4,  8; % 8    ' 4 2 Gate polarity'
                hex2dec('8000'), 11,  8; % 9    '11 1 Open circuit detection A'
                hex2dec('8000'), 12,  8; %10    '12 1 Open circuit detection B'
                hex2dec('8000'), 13,  8; %11    '13 1 Open circuit detection C'
                hex2dec('8000'), 14,  8; %12    '14 1 Reversion of rotation'
                hex2dec('8000'), 16,  8; %13    '16 1 Extern reset polarity'
                hex2dec('8000'), 17, 16; %14    '17 16 Frequency window'
                hex2dec('8000'), 19, 16; %15    '19 16 Frequency scaling'
                hex2dec('8000'), 20, 16; %16    '20 16 Period scaling'
                hex2dec('8000'), 21, 16; %17    '21 16 Frequency resolution'
                hex2dec('8000'), 22, 16; %18    '22 16 Period resolution'
                hex2dec('8000'), 23, 16; %19    '23 16 Frequency wait time'
                hex2dec('8001'),  1, 16; %20    ' 1 16 Frequency window'
                hex2dec('8001'),  2, 16; %21    ' 2 16 Counter reload value'
                hex2dec('8010'),  1,  8; %22    ' 1 1 Enable C reset'
                hex2dec('8010'),  2,  8; %23    ' 2 1 Enable extern reset'
                hex2dec('8010'),  3,  8; %24    ' 3 1 Enable up/down counter'
                hex2dec('8010'),  4,  8; %25    ' 4 2 Gate polarity'
                hex2dec('8010'),  8,  8; %26    ' 8 1 Disable filter'
                hex2dec('8010'), 10,  8; %27    '10 1 Enable micro increments'
                hex2dec('8010'), 11,  8; %28    '11 1 Open circuit detection A'
                hex2dec('8010'), 12,  8; %29    '12 1 Open circuit detection B'
                hex2dec('8010'), 13,  8; %30    '13 1 Open circuit detection C'
                hex2dec('8010'), 14,  8; %31    '14 1 Reversion of rotation'
                hex2dec('8010'), 16,  8; %32    '16 1 Extern reset polarity'
                hex2dec('8010'), 17, 16; %33    '17 16 Frequency window'
                hex2dec('8010'), 19, 16; %34    '19 16 Frequency scaling'
                hex2dec('8010'), 20, 16; %35    '20 16 Period scaling'
                hex2dec('8010'), 21, 16; %36    '21 16 Frequency resolution'
                hex2dec('8010'), 22, 16; %37    '22 16 Period resolution'
                hex2dec('8010'), 23, 16; %38    '23 16 Frequency wait time'
        ];
    end

    properties (Constant)
        %      Model,                   ProductCode, RevNo
        %         Rx,              Tx,                   CoE, AssignActivate
        models = {...
            'EL5101',           hex2dec('13ed3052'), [], ...
                 1:3,            4:11,     [1:5,20,21,22:38], hex2dec('320');
            'EL5101-0010',      hex2dec('13ed3052'), [], ...
                 12:13,         14:19,                  6:19, hex2dec('320');
            'EL5101-1006',      hex2dec('13ed3052'), [], ...
                 1:3,            4:11,                  6:19, hex2dec('320');
            'EL5151',           hex2dec('141f3052'), [], ...
                 12:13, [20,21,16:19],                  6:19, hex2dec('320');
            'EL5151-0080',      hex2dec('141f3052'), [], ...
                 12:13, [20,21,16:19],                  6:19, hex2dec('320');
            'EL5152',           hex2dec('14203052'), [], ...
                 22:25,         26:33, [6:19,22,23,25,28:38], hex2dec('720');
        };
    end
end
