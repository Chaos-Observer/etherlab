function el5021(method, varargin)

if ~nargin
    return
end

%display([gcb ' ' method]);

switch lower(method)
case 'set'
    model = get_param(gcbh, 'model');

    if model(1) ~= 'E'
        errordlg('Please choose a correct slave', gcb);
        return
    end

    ud = get_param(gcbh,'UserData');
    ud.SlaveConfig = slave_config(model);
    ud.PortConfig = port_config(ud.SlaveConfig);
    set_param(gcbh, 'UserData', ud);

case 'check'
    % If UserData.SlaveConfig does not exist, this is an update
    % Convert this block and return
    model = get_param(gcbh,'model');

    ud = get_param(gcbh, 'UserData');

    % Get slave and port configuration based on product code and revision
    sc = slave_config(ud.SlaveConfig.product, ud.SlaveConfig.revision);
    pc = port_config(sc);

    if isequal(sc.sm, ud.SlaveConfig.sm) && ~isequal(sc, ud.SlaveConfig)
        % The slave has a new name
        warning('el5021:NewName', ...
                '%s: Renaming device from %s to %s', ...
                gcb, get_param(gcbh,'model'), sc.description)
        set_param(gcbh, 'model', sc.description)
        return;
    end

    if ~isequal(pc, ud.PortConfig)
        errordlg('Configuration error. Please replace this block', gcb);
        %error('el5021:PortConfig', 'Configuration error on %s. Replace it',...
                %gcb);
    end

case 'update'
    update_devices(varargin{1}, slave_config());

otherwise
    display([gcb, ': Unknown method ', method])
end

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = pdo_config(arg,varargin)

% return a cell array with all pdo's
% each cell contains: {PdoIndex, Excludes, PdoEntryArray}
pdo = {...
        hex2dec('1600'),...
           [ hex2dec('7000'), 1, 1, 1001;
             0              , 0, 1, 0   ;
             hex2dec('7000'), 3, 1, 1001;
             0              , 0,13, 0   ;
             hex2dec('7000'),17,32, 1032],...
           { [0,2], 4 };
        hex2dec('1a00'),...
           [ hex2dec('6000'), 1, 1, 1001;
             0              , 0, 1,    0;
             hex2dec('6000'), 3, 1, 1001;
             hex2dec('6001'), 4, 1, 1001;
             hex2dec('6001'), 5, 1, 1001;
             0              , 0, 5,    0;
             hex2dec('6000'),11, 1, 1001;
             0              , 0, 2,    0;
             hex2dec('1c32'),32, 1, 1001;
             hex2dec('1800'), 7, 1, 1001;
             hex2dec('1800'), 9, 1, 1001;
             hex2dec('6000'),17,32, 1032;
             hex2dec('6000'),18,32, 1032],...
           { [0,2:4,6], 9, 10 };
};

switch arg
case 'TxPdo'
    rv = {pdo(2,[1 2])};

case 'RxPdo'
    rv = {pdo(1,[1 2])};

case 'PortConfig'
    idx = cellfun(@(x) find([pdo{:,1}] == x{1}), varargin{1});
    count = sum(arrayfun(@(x) numel(pdo{x,3}), idx));
    rv = repmat(struct('pdo', []), 1, count);
    count = 1;
    for i = 1:numel(idx)
        spec = pdo{idx(i),3};
        for j = 1:numel(spec)
            n = numel(spec{j});
            rv(count).pdo = repmat([varargin{2},i-1,0,0], numel(spec{j}), 1);
            rv(count).pdo(:,3) = spec{j};
            count = count + 1;
        end
    end
end

return


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = slave_config(varargin)

% directly write the syncmanager configuration
%   Model       ProductCode          Revision          IndexOffset, function
models = {...
  'EL5021',       hex2dec('139d3052'), hex2dec('00110000'); ...
};

switch nargin
case 2
    pos = cell2mat(models(:,2)) == varargin{1}...
        & cell2mat(models(:,3)) == varargin{2};
    product = models(pos,:);

case 1
    product = models(strcmp(models(:,1),varargin{1}),:);

otherwise
    fields = models(:,1);
    obsolete = cellfun(@length, fields) > 11;
    rv = vertcat(sort(fields(~obsolete)), sort(fields(obsolete)));
    return
end

if isempty(product)
    rv = [];
    return;
end

rv.vendor = 2;
rv.description = product{1};
rv.product = product{2};
rv.revision = product{3};

rv.sm = {...
        {2, 0, pdo_config('RxPdo')},...
        {3, 1, pdo_config('TxPdo')},...
};

switch get_param(gcbh,'dcmode')
case 'DC-Synchron'
    rv.dc = [hex2dec('700'),0,1,-30600,0,0,0,1,25000,0];
case 'DC-Synchron (input based)'
    rv.dc = [hex2dec('700'),0,1,-30600,0,1,0,1,25000,0];
case 'DC-Customized'
    %rv.dc = evalin('base',get_param(gcbh,'dccustom'));
end

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = port_config(SlaveConfig)
% Populate the blocks output port(s)
for i = 1:numel(SlaveConfig.sm)
    sm = SlaveConfig.sm{i};
    switch sm{2}
    case 0
        rv.input  = pdo_config('PortConfig',sm{3},i-1);
    case 1
        rv.output = pdo_config('PortConfig',sm{3},i-1);
    end
end

return
