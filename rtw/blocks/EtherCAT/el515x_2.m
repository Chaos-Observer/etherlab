classdef el515x_2 < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function rv = getExcludeList(model, checked)
            slave = el515x_2.findSlave(model,el515x_2.models);
            pdo_list = el515x_2.pdo;

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
        function rv = getCoE(model)
            slave = el515x_2.findSlave(model,el515x_2.models);
            rv = el515x_2.coe;

            switch model(6)
            case '1'
                rv = rv(slave{6},:);
            case '2'
                rv = rv([slave{6}, slave{6}],:);
                rv(numel(slave{6})+1:end,1) = rv(numel(slave{6})+1:end,1) + 16;
            otherwise
                rv = [];
            end
        end

        %====================================================================
        function rv = pdoVisible(model)
            slave = el515x_2.findSlave(model,el515x_2.models);
            pdo_list = el515x_2.pdo;

            rv = [pdo_list{[slave{[4,5]}],1}];
        end 

        %====================================================================
        function rv = configure(model,mapped_pdo,coe8000,coe8010,dc_config)
            slave = el515x_2.findSlave(model,el515x_2.models);

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = model;
            rv.SlaveConfig.product  = slave{2};

            % Get a list of pdo's for the selected slave
            pdo_list = el515x_2.pdo;
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
            inputs = horzcat(inputs{:});

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
            outputs = horzcat(outputs{:});

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
                dc = el515x_2.dc;
                rv.PortConfig.dc = dc(dc_config(1),:);
                rv.PortConfig.dc(1) = slave{7}; % Set AssignActivate
            end

            % CoE Configuration
            coe = el515x_2.coe;
            rv.SlaveConfig.sdo = ...
                num2cell(horzcat(coe(slave{6},:),coe8000(slave{6})'));
            if model(6) == '2'
                coe(:,1) = coe(:,1) + 16;
                rv.SlaveConfig.sdo = vertcat(rv.SlaveConfig.sdo, ...
                        num2cell(horzcat(coe(slave{6},:), coe8010')));
            end
        end

        %====================================================================
        function test(p)
            ei = EtherCATInfo(fullfile(p,'Beckhoff EL5xxx.xml'));
            pdo = el515x_2.pdo;
            for i = 1:size(el515x_2.models,1)
                fprintf('Testing %s\n', el515x_2.models{i,1});
                rv = el515x_2.configure(el515x_2.models{i,1},...
                    hex2dec({'1600', '1601', '1602', '1603',...
                             '1A00', '1A01', '1A02', '1A03',...
                             '1A04', '1A05', '1A06', '1A07'}),...
                             1:17,1:11,1);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);

                rv = el515x_2.configure(el515x_2.models{i,1},...
                    [pdo{[el515x_2.models{i,4},el515x_2.models{i,5}],1}],...
                    1:17,1:11,1);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);

                rv = el515x_2.configure(el515x_2.models{i,1},...
                    [pdo{[el515x_2.models{i,4},el515x_2.models{i,5}],2}],...
                    1:17,1:11,1);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant, Access = private)

        %              PdoEntry       EntryIdx, SubIdx, Bitlen
        pdo = {
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
            hex2dec('1a02'), hex2dec('1a03'), [ hex2dec('6000'),20,32], ...
                   {0, 'Period'};
            hex2dec('1a03'), hex2dec('1a02'), [ hex2dec('6000'),19,32], ...
                   {0, 'Freq'};
            hex2dec('1a04'), hex2dec('1a05'), [ hex2dec('6000'),22,64], ...
                   {0, 'Time'};
            hex2dec('1a05'), hex2dec('1a04'), [ hex2dec('6000'),22,32], ...
                   {0, 'Time'};

            % EL5152
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

        coe = [ hex2dec('8000'), hex2dec( '1'), 8;
                hex2dec('8000'), hex2dec( '2'), 8;
                hex2dec('8000'), hex2dec( '3'), 8;
                hex2dec('8000'), hex2dec( '4'), 8;
                hex2dec('8000'), hex2dec( '8'), 8;
                hex2dec('8000'), hex2dec( 'A'), 8;
                hex2dec('8000'), hex2dec( 'E'), 8;
                hex2dec('8000'), hex2dec( 'F'), 8;
                hex2dec('8000'), hex2dec('10'), 8;
                hex2dec('8000'), hex2dec('11'),16;
                hex2dec('8000'), hex2dec('13'),16;
                hex2dec('8000'), hex2dec('14'),16;
                hex2dec('8000'), hex2dec('15'),16;
                hex2dec('8000'), hex2dec('16'),16;
                hex2dec('8000'), hex2dec('17'),16];
    end

    properties (Constant)
        %   Model           ProductCode
        %                Rx    Tx   CoE,   AssignActivate
        models = {...
            'EL5151',       hex2dec('141f3052'), [], ...
                        1:2,    3:8, 1:15, hex2dec('320');
            'EL5151-0080',  hex2dec('141f3052'), [], ...
                        1:2,    3:8, 1:15, hex2dec('320');
            'EL5152',       hex2dec('14203052'), [], ...
                        9:12, 13:20, [3,5:8,10:15], hex2dec('320');
        };
    end
end
