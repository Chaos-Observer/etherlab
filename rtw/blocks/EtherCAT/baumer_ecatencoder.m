function output = baumer_ecatencoder(command,varargin)

if ~nargin
    return
end

%display([gcb ' ' method]);

switch lower(command)
case 'set'
    model = get_param(gcbh, 'model');

    if model(1) ~= 'B'
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
        warning('BaumerEncoder:NewName', ...
                '%s: Renaming device from %s to %s', ...
                gcb, get_param(gcbh,'model'), sc.description)
        set_param(gcbh, 'model', sc.description)
        return;
    end

    if ~isequal(pc, ud.PortConfig)
        errordlg('Configuration error. Please replace this block', gcb);
    end

case 'ui'
    update_gui;

case 'update'
    update_devices(varargin{1}, slave_config());

otherwise
    display([gcb, ': Unknown method ', command])
end

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = slave_config(varargin)

%   Model       ProductCode Revision 
models = {...
    'BT ATD4',     1,          2;
    'BT ATD2',     2,          2;
    'BT ATD2_PoE', 3,          2;
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
    obsolete = cellfun(@length, fields) > 12;
    rv = vertcat(sort(fields(~obsolete)), sort(fields(obsolete)));
    return
end

if isempty(product)
    rv = [];
    return;
end

rv.vendor = hex2dec('516');
rv.description = product{1};
rv.product = product{2};
rv.revision = product{3};

% Only 1 input SyncManager
rv.sm = {...
        {3, 1, {{hex2dec('1A00') [hex2dec('6004') 0 32 1032]}}}, ...
};

switch get_param(gcbh,'dcmode')
case 'DC On'
    rv.dc = [hex2dec('300') ,0,0,0,0,0,0,0,0,0];
case 'DC Customized'
    rv.dc = evalin('base',get_param(gcbh,'dccustom'));
end

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = port_config(SlaveConfig)
% Populate the blocks output port(s)

rv.output.pdo = [0 0 0 0];

return;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function checked = update_gui

mask_names = get_param(gcbh,'MaskNames');
mask_enables = cell2struct(get_param(gcbh,'MaskEnables'), mask_names);

setting = {'off','on'};
setting = setting{strcmp(get_param(gcbh,'dcmode'),'DC Customized') + 1};

if ~strcmp(mask_enables.dccustom,setting)
    mask_enables.dccustom = setting;
    set_param(gcbh,'MaskEnables',struct2cell(mask_enables));
end

return
