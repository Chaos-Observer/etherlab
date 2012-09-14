function ekxxxx(method, varargin)

if ~nargin
    return
end

%display([gcb ' ' method]);

switch lower(method)
case 'set'
    ud = get_param(gcbh,'UserData');
    model = get_param(gcbh, 'model');

    if model(1) ~= 'E'
        errordlg('Please choose a correct slave', gcb);
        return
    end

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
        warning('el1xxx:NewName', ...
                '%s: Renaming device from %s to %s', ...
                gcb, get_param(gcbh,'model'), sc.description)
        set_param(gcbh, 'model', sc.description)
        return;
    end

    if ~isequal(pc, ud.PortConfig)
        errordlg('Configuration error. Please replace this block', gcb);
        %error('el1xxx:PortConfig', 'Configuration error on %s. Replace it',...
                %gcb);
    end

case 'update'
    update_devices(varargin{1}, slave_config());

otherwise
    display([gcb, ': Unknown method ', method])
end

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = slave_config(varargin)

pdo = [...
        hex2dec('1a00'),  hex2dec('6000'), 1;...
        ];

%   Model       ProductCode          Revision             PdoStartRow PdoEndRow
models = {...
    'EK1100', hex2dec('044c2c52'), hex2dec('00110000'), 0; ...
    'EK1100-0030', hex2dec('044c2c52'), hex2dec('0010001e'), 0; ...
    'EK1101', hex2dec('044d2c52'), hex2dec('00110000'), 1; ...
    'EK1122', hex2dec('04622c52'), hex2dec('00110000'), 0; ...
    'EK1200', hex2dec('04b02c52'), hex2dec('00001388'), 0; ...
    'EK1501', hex2dec('05dd2c52'), hex2dec('00120000'), 1; ...
    'EK1501-0010', hex2dec('05dd2c52'), hex2dec('0011000a'), 1; ...
    'EK1521', hex2dec('05f12c52'), hex2dec('00120000'), 0; ...
    'EK1521-0010', hex2dec('05f12c52'), hex2dec('0012000a'), 0; ...
    'CX1100-0004', hex2dec('044c6032'), hex2dec('00010004'), 0; ...
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
rv.sm = [];

if ~product{4}
    return
end

% Only 1 input SyncManager
rv.sm = {...
        {0, 1, {{pdo(1,1) [pdo(1,2),pdo(1,3),1]}}} ...
};

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = port_config(SlaveConfig)
% Populate the blocks output port(s)

if isempty(SlaveConfig.sm)
    rv = [];
else
    rv.output.pdo = [0,0,0,0];
    rv.output.pdo_data_type = 1001;
end

return
