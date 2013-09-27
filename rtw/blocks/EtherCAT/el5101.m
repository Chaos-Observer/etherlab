classdef el5101 < EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function rv = getExcludeList(model, checked)
            slave = el5101.findSlave(model,el5101.models);
            pdo_list = el5101.pdo;

            % Remove pdo's that do not belong to the slave
            checked = checked(ismember(checked, [pdo_list{:,1}]));

            % Find out which rows are checked
            rows = ismember([pdo_list{:,1}],checked);

            % return column 2 of pdo_list, the exclude list
            rv = [pdo_list{rows,2}];
        end

        %====================================================================
        function rv = getCoE(model)
            rv = el5101.coe8010;
        end

        %====================================================================
        function rv = pdoVisible(model)
            pdo_list = el5101.pdo;
            rv = [pdo_list{:,1}];
        end 

        %====================================================================
        function rv = configure(model,mapped_pdo,...
                        coe8000,coe8001,coe8010,dc_config)
            slave = el5101.findSlave(model,el5101.models);

            rv.SlaveConfig.vendor = 2;
            rv.SlaveConfig.description = model;
            rv.SlaveConfig.product  = slave{2};

            % Get a list of pdo's for the selected slave
            pdo_list = el5101.pdo;
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
                rv.SlaveConfig.dc = dc_config(2:11);
            else
                dc = el5101.dc;
                rv.SlaveConfig.dc = dc(dc_config(1),:);
            end

            % CoE Configuration
            rv.SlaveConfig.sdo = num2cell(...
                [hex2dec('1011'), 1,32,hex2dec('64616f6c');
                 horzcat(el5101.coe8000, coe8000');
                 horzcat(el5101.coe8001, coe8001');
                 horzcat(el5101.coe8010, coe8010')]);
        end

        %====================================================================
        function test(p)
            ei = EtherCATInfo(fullfile(p,'Beckhoff EL5xxx.xml'));
            for i = 1:size(el5101.models,1)
                fprintf('Testing %s\n', el5101.models{i,1});
                pdo = el5101.pdo;
                rv = el5101.configure(el5101.models{i,1},...
                    hex2dec({'1600', '1601', '1602', '1603',...
                             '1A00', '1A01', '1A02', '1A03',...
                             '1A04', '1A05', '1A06', '1A07'}),...
                    1:5,1:2,1:17,1);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);

                rv = el5101.configure(el5101.models{i,1},...
                    [pdo{:,1}], 1:5,1:2,1:17,1);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);

                p = cellfun(@(x) x(1), pdo(:,2));
                rv = el5101.configure(el5101.models{i,1},...
                    p, 1:5,1:2,1:17,1);
                ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant)

        %              PdoEntry       EntryIdx, SubIdx, Bitlen
        pdo = { hex2dec('1601'), hex2dec({'1602','1603'})', ...
                   [ hex2dec('7000'), 1, 8;
                     0              , 0, 8;
                     hex2dec('7000'), 2,16],...
                   { 0, 'Ctrl'; 2, 'Value' };
                hex2dec('1602'), hex2dec({'1601','1603'})', ...
                   [ hex2dec('7010'), 1, 1;
                     hex2dec('7010'), 2, 1;
                     hex2dec('7010'), 3, 1;
                     hex2dec('7010'), 4, 1;
                     0              , 0, 4;
                     0              , 0, 8;
                     hex2dec('7010'),17,16],...
                   { 0:3, 'bool[4]'; 6, 'Value' };
                hex2dec('1603'), hex2dec({'1601','1602'})', ...
                   [ hex2dec('7010'), 1, 1;
                     hex2dec('7010'), 2, 1;
                     hex2dec('7010'), 3, 1;
                     hex2dec('7010'), 4, 1;
                     0              , 0, 4;
                     0              , 0, 8;
                     hex2dec('7010'),17,32],...
                   { 0:3, 'bool[4]'; 6, 'Value' };
                hex2dec('1a01'), hex2dec({'1a03','1a04','1a05',...
                                          '1a06','1a07','1a08'})',...
                   [ hex2dec('6000'), 1, 8;
                     0              , 0, 8;
                     hex2dec('6000'), 2,16;
                     hex2dec('6000'), 3,16],...
                   { 0, 'Status'; 2, 'Counter'; 3, 'Latch' };
                hex2dec('1a02'), hex2dec({'1a03','1a04','1a05',...
                                          '1a06','1a07','1a08'})',...
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
        };

        dc = [hex2dec('320'),0,0,0,0,0,0,0,0,0;      % FreeRun
              hex2dec('320'),0,1,0,0,0,0,1,0,0;      % DC-Synchron
              hex2dec('320'),0,1,0,0,1,0,1,0,0];     % DC-Synchron (input based)

        coe8000 = [ hex2dec('8000'),          1,  8;
                    hex2dec('8000'),          2,  8;
                    hex2dec('8000'),          3,  8;
                    hex2dec('8000'),          4,  8;
                    hex2dec('8000'),          5,  8];

        coe8001 = [ hex2dec('8001'),          1, 16;
                    hex2dec('8001'),          2, 16];

        coe8010 = [ hex2dec('8000'),          1,   8;
                    hex2dec('8000'),          2,   8;
                    hex2dec('8000'),          3,   8;
                    hex2dec('8000'),          4,   8;
                    hex2dec('8000'),          8,   8;
                    hex2dec('8000'),hex2dec('0A'), 8;
                    hex2dec('8000'),hex2dec('0B'), 8;
                    hex2dec('8000'),hex2dec('0C'), 8;
                    hex2dec('8000'),hex2dec('0D'), 8;
                    hex2dec('8000'),hex2dec('0E'), 8;
                    hex2dec('8000'),hex2dec('10'), 8;
                    hex2dec('8000'),hex2dec('11'),16;
                    hex2dec('8000'),hex2dec('13'),16;
                    hex2dec('8000'),hex2dec('14'),16;
                    hex2dec('8000'),hex2dec('15'),16;
                    hex2dec('8000'),hex2dec('16'),16;
                    hex2dec('8000'),hex2dec('17'),16];
    end

    properties (Constant)
        %   Model           ProductCode
        %                Rx    Tx   CoE,   AssignActivate
        models = {...
            'EL5101',       hex2dec('13ed3052'), [], 1:3, 4:11;
            'EL5101-1006',  hex2dec('13ed3052'), [], 1:3, 4:11;
        };
    end
end
%function el5101(method, varargin)
%
%if ~nargin
%    return
%end
%
%%display([gcb ' ' method]);
%
%switch lower(method)
%case 'set'
%    model = get_param(gcbh, 'model');
%
%    if model(1) ~= 'E'
%        errordlg('Please choose a correct slave', gcb);
%        return
%    end
%
%    ud = get_param(gcbh,'UserData');
%    ud.SlaveConfig = slave_config(model);
%    ud.PortConfig = port_config(ud.SlaveConfig);
%    set_param(gcbh, 'UserData', ud);
%
%case 'check'
%    % If UserData.SlaveConfig does not exist, this is an update
%    % Convert this block and return
%    model = get_param(gcbh,'model');
%
%    ud = get_param(gcbh, 'UserData');
%
%    % Get slave and port configuration based on product code and revision
%    sc = slave_config(ud.SlaveConfig.product, ud.SlaveConfig.revision);
%    pc = port_config(sc);
%
%    if isequal(sc.sm, ud.SlaveConfig.sm) && ~isequal(sc, ud.SlaveConfig)
%        % The slave has a new name
%        warning('el5101:NewName', ...
%                '%s: Renaming device from %s to %s', ...
%                gcb, get_param(gcbh,'model'), sc.description)
%        set_param(gcbh, 'model', sc.description)
%        return;
%    end
%
%    if ~isequal(pc, ud.PortConfig)
%        errordlg('Configuration error. Please replace this block', gcb);
%        %error('el5101:PortConfig', 'Configuration error on %s. Replace it',...
%                %gcb);
%    end
%
%case 'update'
%    update_devices(varargin{1}, slave_config());
%
%case 'ui'
%    update_gui;
%
%otherwise
%    display([gcb, ': Unknown method ', method])
%end
%
%return
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%function rv = pdo_config(arg,varargin)
%
%% return a cell array with all pdo's
%% each cell contains: {PdoIndex, Excludes, PdoEntryArray}
%pdo = { hex2dec('1601'),...
%           hex2dec({'1602','1603'}), ...
%           [ hex2dec('7000'), 1, 8;
%             0              , 0, 8;
%             hex2dec('7000'), 2,16],...
%           { 0, 2 };
%        hex2dec('1602'),...
%           hex2dec({'1601','1603'}), ...
%           [ hex2dec('7010'), 1, 1;
%             hex2dec('7010'), 2, 1;
%             hex2dec('7010'), 3, 1;
%             hex2dec('7010'), 4, 1;
%             0              , 0, 4;
%             0              , 0, 8;
%             hex2dec('7010'),17,16],...
%           { 0:3, 6 };
%        hex2dec('1603'),...
%           hex2dec({'1601','1602'}), ...
%           [ hex2dec('7010'), 1, 1;
%             hex2dec('7010'), 2, 1;
%             hex2dec('7010'), 3, 1;
%             hex2dec('7010'), 4, 1;
%             0              , 0, 4;
%             0              , 0, 8;
%             hex2dec('7010'),17,32],...
%           { 0:3, 6 };
%        hex2dec('1a01'),...
%           hex2dec({'1a03','1a04','1a05','1a06','1a07','1a08'}),...
%           [ hex2dec('6000'), 1, 8;
%             0              , 0, 8;
%             hex2dec('6000'), 2,16;
%             hex2dec('6000'), 3,16],...
%           { 0, 2, 3 };
%        hex2dec('1a02'),...
%           hex2dec({'1a03','1a04','1a05','1a06','1a07','1a08'}),...
%           [ hex2dec('6000'), 4,32;
%             hex2dec('6000'), 5,16;
%             hex2dec('6000'), 6,16],...
%           { 0, 1, 2 };
%        hex2dec('1a03'),...
%           hex2dec({'1a01','1a02','1a04'}),...
%           [ hex2dec('6010'), 1, 1;
%             hex2dec('6010'), 2, 1;
%             hex2dec('6010'), 3, 1;
%             hex2dec('6010'), 4, 1;
%             hex2dec('6010'), 5, 1;
%             hex2dec('6010'), 6, 1;
%             hex2dec('6010'), 7, 1;
%             hex2dec('6010'), 8, 1;
%             hex2dec('6010'), 9, 1;
%             hex2dec('6010'),10, 1;
%             hex2dec('6010'),11, 1;
%             hex2dec('6010'),12, 1;
%             hex2dec('6010'),13, 1;
%             hex2dec('1c32'),32, 1;
%             hex2dec('1803'), 7, 1;
%             hex2dec('1803'), 9, 1;
%             hex2dec('6010'),17,16;
%             hex2dec('6010'),18,16],...
%           { 0:12, 16, 17 };
%        hex2dec('1a04'),...
%           hex2dec({'1a01','1a02','1a03'}),...
%           [ hex2dec('6010'), 1, 1;
%             hex2dec('6010'), 2, 1;
%             hex2dec('6010'), 3, 1;
%             hex2dec('6010'), 4, 1;
%             hex2dec('6010'), 5, 1;
%             hex2dec('6010'), 6, 1;
%             hex2dec('6010'), 7, 1;
%             hex2dec('6010'), 8, 1;
%             hex2dec('6010'), 9, 1;
%             hex2dec('6010'),10, 1;
%             hex2dec('6010'),11, 1;
%             hex2dec('6010'),12, 1;
%             hex2dec('6010'),13, 1;
%             hex2dec('1c32'),32, 1;
%             hex2dec('1804'), 7, 1;
%             hex2dec('1804'), 9, 1;
%             hex2dec('6010'),17,32;
%             hex2dec('6010'),18,32],...
%           { 0:12, 16, 17 };
%        hex2dec('1a05'),...
%           hex2dec({'1a01','1a02','1a06'}),...
%           [ hex2dec('6010'),19,32],...
%           { 0 };
%        hex2dec('1a06'),...
%           hex2dec({'1a01','1a02','1a05'}),...
%           [ hex2dec('6010'),20,32],...
%           { 0 };
%        hex2dec('1a07'),...
%           hex2dec({'1a01','1a02','1a08'}),...
%           [ hex2dec('6010'),22,64],...
%           { 0 };
%        hex2dec('1a08'),...
%           hex2dec({'1a01','1a02','1a07'}),...
%           [ hex2dec('6010'),22,32],...
%           { 0 };
%};
%
%index = [pdo{:,1}];
%selected = arrayfun(@(x) strcmp(get_param(gcbh,['x',dec2hex(x)]),'on'), index);
%
%switch arg
%case {'TxPdo', 'RxPdo'}
%    % Return a cell vector {PdoIndex [PdoEntries]} of all selected pdos
%    selected = selected & (  arg(1) == 'T' & index >= hex2dec('1a00') ...
%                           | arg(1) == 'R' & index <  hex2dec('1a00'));
%    rv = arrayfun(@(x) pdo(x,[1 3]), find(selected), 'UniformOutput', false);
%
%case 'Exclude'
%    exclude = ismember(index,unique(vertcat(pdo{selected,2})));
%    onoff = {'on','off'};
%    rv = vertcat(num2cell(index),onoff(exclude+1));
%
%case 'PortConfig'
%    idx = cellfun(@(x) find([pdo{:,1}] == x{1}), varargin{1});
%    count = sum(arrayfun(@(x) numel(pdo{x,4}), idx));
%    rv = repmat(struct('pdo', [], 'pdo_data_type', 0), 1, count);
%    count = 1;
%    for i = 1:numel(idx)
%        spec = pdo{idx(i),4};
%        for j = 1:numel(spec)
%            n = numel(spec{j});
%            rv(count).pdo = repmat([varargin{2},i-1,0,0], numel(spec{j}), 1);
%            rv(count).pdo(:,3) = spec{j};
%            rv(count).pdo_data_type = 1000 + pdo{idx(i),3}(spec{i}(1)+1,3);
%            count = count + 1;
%        end
%    end
%end
%
%return
%
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%function rv = slave_config(varargin)
%
%% directly write the syncmanager configuration
%%   Model       ProductCode          Revision          IndexOffset, function
%models = {...
%  'EL5101',       hex2dec('13ed3052'), hex2dec('03fd0000'); ...
%  'EL5101-1006',  hex2dec('13ed3052'), hex2dec('03fd03ee'); ...
%};
%
%switch nargin
%case 2
%    pos = cell2mat(models(:,2)) == varargin{1}...
%        & cell2mat(models(:,3)) == varargin{2};
%    product = models(pos,:);
%
%case 1
%    product = models(strcmp(models(:,1),varargin{1}),:);
%
%otherwise
%    fields = models(:,1);
%    obsolete = cellfun(@length, fields) > 11;
%    rv = vertcat(sort(fields(~obsolete)), sort(fields(obsolete)));
%    return
%end
%
%if isempty(product)
%    rv = [];
%    return;
%end
%
%rv.vendor = 2;
%rv.description = product{1};
%rv.product = product{2};
%rv.revision = product{3};
%
%rv.sm = {...
%        {2, 0, pdo_config('RxPdo')},...
%        {3, 1, pdo_config('TxPdo')},...
%};
%
%switch get_param(gcbh,'dcmode')
%case 'DC-Synchron'
%    rv.dc = [hex2dec('320'),0,1,0,0,0,0,1,0,0];
%case 'DC-Synchron (input based)'
%    rv.dc = [hex2dec('320'),0,1,0,0,1,0,1,0,0];
%case 'DC-Customized'
%    %rv.dc = evalin('base',get_param(gcbh,'dccustom'));
%end
%
%return
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%function rv = port_config(SlaveConfig)
%% Populate the blocks output port(s)
%
%for i = 1:numel(SlaveConfig.sm)
%    sm = SlaveConfig.sm{i};
%    switch sm{2}
%    case 0
%        rv.input  = pdo_config('PortConfig',sm{3},i-1);
%    case 1
%        rv.output = pdo_config('PortConfig',sm{3},i-1);
%    end
%end
%
%return
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%function checked = update_gui
%
%mask_enables = cell2struct(...
%    get_param(gcbh,'MaskEnables'),...
%    get_param(gcbh,'MaskNames')...
%);
%
%old_enables = mask_enables;
%
%exclude = pdo_config('Exclude');
%
%for i = 1:size(exclude,2)
%    mask_enables.(strcat('x',dec2hex(exclude{1,i}))) = exclude{2,i};
%end
%
%if ~isequal(old_enables, mask_enables)
%    set_param(gcbh,'MaskEnables',struct2cell(mask_enables));
%end
%
%return
