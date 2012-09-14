function el4xxx(method, varargin)

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
        warning('el4xxx:NewName', ...
                '%s: Renaming device from %s to %s', ...
                gcb, get_param(gcbh,'model'), sc.description)
        set_param(gcbh, 'model', sc.description)
        return;
    end

    if ~isequal(pc, ud.PortConfig)
        errordlg('Configuration error. Please replace this block', gcb);
        %error('el4xxx:PortConfig', 'Configuration error on %s. Replace it',...
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

%      PdoIdx            PdoEntry
% Note: the PdoIdx field will be incremented by IndexOffset (column 3)
%       from the models struct below
pdo = [...
        hex2dec('1600'), hex2dec('7000'), 17; ...
        hex2dec('1601'), hex2dec('7010'), 17; ...
        hex2dec('1602'), hex2dec('7020'), 17; ...
        hex2dec('1603'), hex2dec('7030'), 17; ...
        hex2dec('1604'), hex2dec('7040'), 17; ...
        hex2dec('1605'), hex2dec('7050'), 17; ...
        hex2dec('1606'), hex2dec('7060'), 17; ...
        hex2dec('1607'), hex2dec('7070'), 17; ...
        hex2dec('1600'), hex2dec('6411'),  1; ...
        hex2dec('1601'), hex2dec('6411'),  2; ...
        ];

%   Model       ProductCode          Revision          IndexOffset, function
models = {...
  'EL4001', hex2dec('0fa13052'), hex2dec('00100000'), 1,  '0-10V'; ...
  'EL4002', hex2dec('0fa23052'), hex2dec('00100000'), 1,  '0-10V'; ...
  'EL4004', hex2dec('0fa43052'), hex2dec('00100000'), 1,  '0-10V'; ...
  'EL4008', hex2dec('0fa83052'), hex2dec('00100000'), 1,  '0-10V'; ...
  'EL4011', hex2dec('0fab3052'), hex2dec('00100000'), 1, '0-20mA'; ...
  'EL4012', hex2dec('0fac3052'), hex2dec('00100000'), 1, '0-20mA'; ...
  'EL4014', hex2dec('0fae3052'), hex2dec('00100000'), 1, '0-20mA'; ...
  'EL4018', hex2dec('0fb23052'), hex2dec('00100000'), 1, '0-20mA'; ...
  'EL4021', hex2dec('0fb53052'), hex2dec('00100000'), 1, '4-20mA'; ...
  'EL4022', hex2dec('0fb63052'), hex2dec('00100000'), 1, '4-20mA'; ...
  'EL4024', hex2dec('0fb83052'), hex2dec('00100000'), 1, '4-20mA'; ...
  'EL4028', hex2dec('0fbc3052'), hex2dec('00100000'), 1, '4-20mA'; ...
  'EL4031', hex2dec('0fbf3052'), hex2dec('00100000'), 1, '+/-10V'; ...
  'EL4032', hex2dec('0fc03052'), hex2dec('00100000'), 1, '+/-10V'; ...
  'EL4034', hex2dec('0fc23052'), hex2dec('00100000'), 1, '+/-10V'; ...
  'EL4038', hex2dec('0fc63052'), hex2dec('00100000'), 1, '+/-10V'; ...
  'EL4102', hex2dec('10063052'), hex2dec('03fa0000'), 9,  '0-10V'; ...
  'EL4104', hex2dec('10083052'), hex2dec('03f90000'), 1,  '0-10V'; ...
  'EL4112', hex2dec('10103052'), hex2dec('03fa0000'), 9, '0-20mA'; ...
  'EL4112-0010', hex2dec('10103052'), hex2dec('03fa000a'), 9, '+/-10mA'; ...
  'EL4114', hex2dec('10123052'), hex2dec('03f90000'), 1, '0-20mA'; ...
  'EL4122', hex2dec('101a3052'), hex2dec('03fa0000'), 9, '4-20mA'; ...
  'EL4124', hex2dec('101c3052'), hex2dec('03f90000'), 1, '4-20mA'; ...
  'EL4132', hex2dec('10243052'), hex2dec('03fa0000'), 9, '+/-10V'; ...
  'EL4134', hex2dec('10263052'), hex2dec('03f90000'), 1, '+/-10V'; ...
};

dc = {'Sm-Synchron', 0, 0, 0, 0, 0, 0, 0, 1, 0, 0;...
        'DC-Synchron',  hex2dec('700'), 0, 1, 0, 0, 0, 0, 1, 10000, 0;...
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
rv.function = product{5};

% Find out which rows of pdo matrix we need
rows = 0:str2double(rv.description(6))-1;

% Populate the PDO structure
rv.sm = {{2, 0, {}}};
rv.sm{1}{3} = arrayfun(...
    @(x) {pdo(x,1) [pdo(x,2), pdo(x,3), 16]},...
    rows + product{4}, 'UniformOutput', 0);

dc_idx = find(strcmp(get_param(gcbh,'dc'), dc(:,1)));
if (~isempty(dc_idx) && dc_idx > 1)
    rv.dc = cell2mat(dc(dc_idx, 2:end));
end

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = get_val(field)
str = get_param(gcbh, field);
rv = [];
if isempty(str)
    return
end
rv = evalin('base',str);
if ~isnumeric(rv)
    rv = [];
    error(['el4xxx:',field], 'Value is not numeric');
end

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = port_config(SlaveConfig)
% Populate the blocks input port(s)

vector  = strcmp(get_param(gcbh,'vector'), 'on');
raw     = strcmp(get_param(gcbh,'raw'), 'on');
pdo_count = numel(SlaveConfig.sm{1}{3});

gain   = get_val('scale');

if (vector)
    rv.input.pdo      = zeros(pdo_count, 4);
    rv.input.pdo(:,2) = 0:pdo_count-1;
    rv.input.pdo_data_type = 2016;

    if ~raw
        rv.input.full_scale = 2^15;

        if ~isempty(gain)
            rv.input.gain = {'Gain', gain};
        end
    end

else

    rv.input = repmat(struct('pdo',[0 0 0 0],...
                             'pdo_data_type', 2016), 1, pdo_count);

    if ~raw
        if (numel(gain) == 1)
            gain = repmat(gain,1,pdo_count);
        end
    end

    for i = 1:pdo_count
        rv.input(i).pdo(2)  = i-1;

        if ~raw

            rv.input(i).full_scale = 2^15;

            if i <= numel(gain)
                rv.input(i).gain = {['Gain', int2str(i)], gain(i)};
            end
        end
    end
end

return
