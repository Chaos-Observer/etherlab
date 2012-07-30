function el500x(method, varargin)

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

    update_gui

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
        warning('el5001:NewName', ...
                '%s: Renaming device from %s to %s', ...
                gcb, get_param(gcbh,'model'), sc.description)
        set_param(gcbh, 'model', sc.description)
        return;
    end

    if ~isequal(pc, ud.PortConfig)
        errordlg('Configuration error. Please replace this block', gcb);
        %error('el5001:PortConfig', 'Configuration error on %s. Replace it',...
                %gcb);
    end

case 'update'
    update_devices(varargin{1}, slave_config());

case 'ui'
    update_gui;

otherwise
    display([gcb, ': Unknown method ', method])
end

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = slave_config(varargin)

% directly write the syncmanager configuration
pdo = { {hex2dec('1a00') [hex2dec('6000'), 1, 1, 1001;...
                          hex2dec('6000'), 2, 1, 1001;...
                          hex2dec('6000'), 3, 1, 1001;...
                          0              , 0,10, 0   ;...
                          hex2dec('1c32'),32, 1, 1001;...
                          hex2dec('1800'), 7, 1, 1001;...
                          hex2dec('1800'), 9, 1, 1001;...
                          hex2dec('6000'),17,32, 1032]}, ...
        {hex2dec('1a01') [hex2dec('6010'), 1, 1, 1001;...
                          hex2dec('6010'), 2, 1, 1001;...
                          hex2dec('6010'), 3, 1, 1001;...
                          0              , 0,10, 0   ;...
                          hex2dec('1c32'),32, 1, 1001;...
                          hex2dec('1801'), 7, 1, 1001;...
                          hex2dec('1801'), 9, 1, 1001;...
                          hex2dec('6010'),17,32, 1032]},...
        {hex2dec('1a00') [hex2dec('6000'), 1, 1, 1001;...
                          hex2dec('6000'), 2, 1, 1001;...
                          hex2dec('6000'), 3, 1, 1001;...
                          0              , 0, 5, 0   ;...
                          0              , 0, 5, 0   ;...
                          hex2dec('6000'),14, 1, 1001;...
                          hex2dec('1800'), 7, 1, 1001;...
                          hex2dec('1800'), 9, 1, 1001;...
                          hex2dec('6000'),17,32, 1032]},...
        {hex2dec('1a02') [hex2dec('6000'),18,32, 1032]}};

%   Model       ProductCode          Revision          IndexOffset, function
models = {...
  'EL5001',       hex2dec('13893052'), hex2dec('03f90000'), [2]; ...
  'EL5001-0011',  hex2dec('13893052'), hex2dec('0010000b'), [3 4]; ...
  'EL5002',       hex2dec('138a3052'), hex2dec('00110000'), [1 2]; ...
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
rv.sm = {{3, 1, pdo(product{4})}};
rv.ssi_monitor = strncmp(rv.description,'EL5001-001',10);

switch get_param(gcbh,'dcmode')
case 'DC-Synchron'
    rv.dc = [hex2dec('700'),0,1,0,0,0,0,1,15000,0];
case 'DC-Synchron (input based)'
    rv.dc = [hex2dec('700'),0,1,0,0,1,0,1,15000,0];
case 'DC-Latch active (only for EL5001-001x)'
    rv.dc = [hex2dec('120'),0,0,0,0,0,0,1,0,0];
case 'DC-Customized'
    rv.dc = evalin('base',get_param(gcbh,'dccustom'));
end

% Remove Timestamp PDO if Monitor Terminal does not require it
if rv.ssi_monitor && strcmp(get_param(gcbh,'timestamp'),'off')
    rv.sm{1}{3}(2) = [];
end

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = port_config(SlaveConfig)
% Populate the blocks output port(s)

last_pdo = size(SlaveConfig.sm{1}{3}{1}{2},1) - 1;

if SlaveConfig.ssi_monitor
    rv.output(1).pdo = [0,0,last_pdo,0];
    if strcmp(get_param(gcbh,'timestamp'),'on')
        rv.output(2).pdo = [0,1,0,0];
    end
    rv.output(end+1).pdo = [0,0,0,0];
    rv.output(end+1).pdo = [0,0,1,0];
    if strcmp(get_param(gcbh,'power_fail1'),'on')
        rv.output(end+1).pdo = [0,0,2,0];
    end
else
    pdo_count = numel(SlaveConfig.sm{1}{3});

    m = zeros(pdo_count,4);
    m(:,2) = 0:pdo_count-1;

    rv.output = repmat(struct('pdo', m), 1, 4);

    i = ones(pdo_count,1);

    rv.output(1).pdo(:,3) = last_pdo * i;
    rv.output(2).pdo(:,3) = 0*i;
    rv.output(3).pdo(:,3) = i;
    rv.output(4).pdo(:,3) = 2*i;

    if (pdo_count == 1 && strcmp(get_param(gcbh,'power_fail1'),'off')) ...
            || (pdo_count == 2 && strcmp(get_param(gcbh,'power_fail1'),'off') ...
                               && strcmp(get_param(gcbh,'power_fail2'),'off')) ...
        rv.output(4) = [];
    end
end

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function update_gui
mask_enables = cell2struct(...
    get_param(gcbh,'MaskEnables'),...
    get_param(gcbh,'MaskNames')...
);

old_enables = mask_enables;

choice = {'off','on'};
value = choice{strcmp(get_param(gcbh,'dcmode'),'DC-Customized') + 1};

if ~strcmp(mask_enables.dccustom,value)
    mask_enables.dccustom = value;
end

value = choice{strcmp(get_param(gcbh,'frame1'),...
                      'Variable (set size in Frame Size)') + 1};
if ~strcmp(mask_enables.fsize1,value)
    mask_enables.fsize1 = value;
end
if ~strcmp(mask_enables.length1,value)
    mask_enables.length1 = value;
end

value = choice{strcmp(get_param(gcbh,'frame2'),...
                      'Variable (set size in Frame Size)') + 1};
if ~strcmp(mask_enables.fsize2,value)
    mask_enables.fsize2 = value;
end
if ~strcmp(mask_enables.length2,value)
    mask_enables.length2 = value;
end

if ~isequal(old_enables, mask_enables)
    set_param(gcbh,'MaskEnables',struct2cell(mask_enables));
end
