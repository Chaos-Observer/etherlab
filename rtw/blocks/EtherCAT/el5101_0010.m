classdef el5101_0010 < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function rv = getExcludeList(model, checked)
            slave = el5101_0010.findSlave(model,el5101_0010.models);
            pdo_list = el5101_0010.pdo;

            % Remove pdo's that do not belong to the slave
            checked = checked(ismember(checked, [pdo_list{:,1}]));

            % Find out which rows are checked
            rows = ismember([pdo_list{:,1}],checked);

            % return column 2 of pdo_list, the exclude list
            rv = [pdo_list{rows,2}];
        end

        %====================================================================
        function rv = getCoE(model)
            rv = el5101_0010.coe;
        end

        %====================================================================
        function rv = pdoVisible(model)
            pdo_list = el5101_0010.pdo;
            rv = [pdo_list{:,1}];
        end 

        %====================================================================
        function rv = configure(model,mapped_pdo,coe8000,dc_config)
            slave = el5101_0010.findSlave(model,el5101_0010.models);

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = model;
            rv.SlaveConfig.product  = slave{2};

            % Get a list of pdo's for the selected slave
            pdo_list = el5101_0010.pdo;
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
                {{2,0, arrayfun(@(x) {pdo_list{x,1}, pdo_list{x,3}},...
                                rx, 'UniformOutput', false)}, ...
                 {3,1, arrayfun(@(x) {pdo_list{x,1}, pdo_list{x,3}},...
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
                dc = el5101_0010.dc;
                rv.PortConfig.dc = dc(dc_config(1),:);
            end

            % CoE Configuration
            rv.PortConfig.coe = num2cell(...
                [hex2dec('1011'), 1,32,hex2dec('64616f6c');
                 horzcat(el5101_0010.coe, coe8000')]);
        end

        %====================================================================
        function test(p)
            ei = EtherCATInfo(fullfile(p,'Beckhoff EL5xxx.xml'));
            for i = 1:size(el5101_0010.models,1)
                fprintf('Testing %s\n', el5101_0010.models{i,1});
                pdo = el5101_0010.pdo;
                rv = el5101_0010.configure(el5101_0010.models{i,1},...
                    hex2dec({'1600', '1601', '1602', '1603',...
                             '1A00', '1A01', '1A02', '1A03',...
                             '1A04', '1A05', '1A06', '1A07'}),...
                    1:14,1);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);

                rv = el5101_0010.configure(el5101_0010.models{i,1},...
                    [pdo{:,1}], 1:14,1);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);

                rv = el5101_0010.configure(el5101_0010.models{i,1},...
                    [pdo{:,2}], 1:14,1);
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
        };

        dc = [hex2dec('320'),0,0,0,0,0,0,0,0,0;      % FreeRun
              hex2dec('320'),0,1,0,0,0,0,1,0,0;      % DC-Synchron
              hex2dec('320'),0,1,0,0,1,0,1,0,0];     % DC-Synchron (input based)

        coe = [ hex2dec('8000'), 1, 8;
                hex2dec('8000'), 2, 8;
                hex2dec('8000'), 4, 8;
                hex2dec('8000'),11, 8;
                hex2dec('8000'),12, 8;
                hex2dec('8000'),13, 8;
                hex2dec('8000'),14, 8;
                hex2dec('8000'),16, 8;
                hex2dec('8000'),17,16;
                hex2dec('8000'),19,16;
                hex2dec('8000'),20,16;
                hex2dec('8000'),21,16;
                hex2dec('8000'),22,16;
                hex2dec('8000'),23,16];
    end

    properties (Constant)
        %   Model           ProductCode
        %                Rx    Tx   CoE,   AssignActivate
        models = {...
            'EL5101-0010',  hex2dec('13ed3052'), [], 1:2, 3:8;
        };
    end
end
