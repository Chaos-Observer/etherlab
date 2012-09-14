function ep1xxx(method, varargin)

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
        warning('ep1xxx:NewName', ...
                '%s: Renaming device from %s to %s', ...
                gcb, get_param(gcbh,'model'), sc.description)
        set_param(gcbh, 'model', sc.description)
        return;
    end

    if ~isequal(pc, ud.PortConfig)
        errordlg('Configuration error. Please replace this block', gcb);
        %error('ep1xxx:PortConfig', 'Configuration error on %s. Replace it',...
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

        % PdoIdx        PdoSubIdx       BitLen, DataType
pdo = [...
        hex2dec('1a00'),  hex2dec('6000'),  1; ...
        hex2dec('1a01'),  hex2dec('6010'),  1; ...
        hex2dec('1a02'),  hex2dec('6020'),  1; ...
        hex2dec('1a03'),  hex2dec('6030'),  1; ...
        hex2dec('1a04'),  hex2dec('6040'),  1; ...
        hex2dec('1a05'),  hex2dec('6050'),  1; ...
        hex2dec('1a06'),  hex2dec('6060'),  1; ...
        hex2dec('1a07'),  hex2dec('6070'),  1; ...
        hex2dec('1a08'),  hex2dec('6080'),  1; ...
        hex2dec('1a09'),  hex2dec('6090'),  1; ...
        hex2dec('1a0a'),  hex2dec('60a0'),  1; ...
        hex2dec('1a0b'),  hex2dec('60b0'),  1; ...
        hex2dec('1a0c'),  hex2dec('60c0'),  1; ...
        hex2dec('1a0d'),  hex2dec('60d0'),  1; ...
        hex2dec('1a0e'),  hex2dec('60e0'),  1; ...
        hex2dec('1a0f'),  hex2dec('60f0'),  1; ...
        hex2dec('1a00'),  hex2dec('f600'), 16; ...
        ];

%   Model       ProductCode          Revision       PdoStartRow Description
models = {...
    'EP1008-0001',hex2dec('03f04052'),hex2dec('00110001'), 1,'8 Ch DigIn'; ...
    'EP1008-0002',hex2dec('03f04052'),hex2dec('00110002'), 1,'8 Ch DigIn'; ...
    'EP1018-0001',hex2dec('03fa4052'),hex2dec('00110001'), 1,'8 Ch DigIn'; ...
    'EP1018-0002',hex2dec('03fa4052'),hex2dec('00110002'), 1,'8 Ch DigIn'; ...
    'EP1111'     ,hex2dec('04574052'),hex2dec('00110000'),17,'ID-Switch'; ...
    'EP1122-0001',hex2dec('04624052'),hex2dec('00110001'), 0,'Junction'; ...
    'EP1809-0021',hex2dec('07114052'),hex2dec('00100015'), 1,'16 Ch DigIn'; ...
    'EP1809-0022',hex2dec('07114052'),hex2dec('00100016'), 1,'16 Ch DigIn'; ...
    'EP1819-0022',hex2dec('071b4052'),hex2dec('00100016'), 1,'16 Ch DigIn'; ...
    };
    %'EP1258-0001',hex2dec('04ea4052'),hex2dec('00110001'), 1,'8 Ch DigIn'; ...
    %'EP1258-0002',hex2dec('04ea4052'),hex2dec('00110002'), 1,'8 Ch DigIn'; ...
    %'EP1816-0008',hex2dec('07184052'),hex2dec('00100016'), 1,'16 Ch DigIn'; ...

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

if ~product{4}
    return
end

% Find out which rows of pdo matrix we need
channels = str2double(rv.description(6));
if channels == 9
    % Models that end in '9' have 16 channels
    channels = 16;
end
rows = product{4}:product{4}+channels-1;

rv.sm = {...
        {0, 1, {}} ...  % Only 1 input SyncManager
};

% Populate the PDO structure
rv.sm{1}{3} = arrayfun(...
        @(x) {pdo(x,1), [pdo(x,2),1,pdo(x,3)]}, rows,...
        'UniformOutput', false);
return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = port_config(SlaveConfig)
% Populate the blocks output port(s)

rv = [];
try
    r = 0:(numel(SlaveConfig.sm{1}{3})-1);
catch
    return;
end

if strcmp(get_param(gcbh,'vector'), 'on')
    rv.output.pdo = zeros(numel(r),4);
    rv.output.pdo(:,2) = r;
    rv.output.pdo_data_type = 1001;
else
    rv.output = arrayfun(@(x) struct('pdo', [0, x, 0, 0], ...
                                     'pdo_data_type', 1001), r);
end

return
