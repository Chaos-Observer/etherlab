function el2xxx(method, varargin)

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
    update_gui(numel(ud.SlaveConfig.sm) > 1);

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
        warning('el2xxx:NewName', ...
                '%s: Renaming device from %s to %s', ...
                gcb, get_param(gcbh,'model'), sc.description)
        set_param(gcbh, 'model', sc.description)
        return;
    end

    if ~isequal(pc, ud.PortConfig)
        errordlg('Configuration error. Please replace this block', gcb);
        %error('el2xxx:PortConfig', 'Configuration error on %s. Replace it',...
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

%      Sm   PdoIdx          EntryIdx      SubIdx
pdo = [...
        0, hex2dec('1600'), hex2dec('7000'), 1;...  %   1
        0, hex2dec('1601'), hex2dec('7010'), 1;...  %   2
        0, hex2dec('1602'), hex2dec('7020'), 1;...  %   3
        0, hex2dec('1603'), hex2dec('7030'), 1;...  %   4
        0, hex2dec('1604'), hex2dec('7040'), 1;...  %   5
        0, hex2dec('1605'), hex2dec('7050'), 1;...  %   6
        0, hex2dec('1606'), hex2dec('7060'), 1;...  %   7
        0, hex2dec('1607'), hex2dec('7070'), 1;...  %   8
        1, hex2dec('1608'), hex2dec('7080'), 1;...  %   9
        1, hex2dec('1609'), hex2dec('7090'), 1;...  %  10
        1, hex2dec('160a'), hex2dec('70a0'), 1;...  %  11
        1, hex2dec('160b'), hex2dec('7030'), 1;...  %  12
        1, hex2dec('160c'), hex2dec('70c0'), 1;...  %  13
        1, hex2dec('160d'), hex2dec('70d0'), 1;...  %  14
        1, hex2dec('160e'), hex2dec('70e0'), 1;...  %  15
        1, hex2dec('160f'), hex2dec('70f0'), 1;...  %  16

        1, hex2dec('1a00'), hex2dec('6000'), 1;...  %  17
        1, hex2dec('1a01'), hex2dec('6010'), 1;...  %  18
        1, hex2dec('1a02'), hex2dec('6020'), 1;...  %  19
        1, hex2dec('1a03'), hex2dec('6030'), 1;...  %  20

        0, hex2dec('1600'), hex2dec('3001'), 1;...  %  21
        0, hex2dec('1601'), hex2dec('3001'), 2;...  %  22
        0, hex2dec('1602'), hex2dec('3001'), 3;...  %  23
        0, hex2dec('1603'), hex2dec('3001'), 4;...  %  24

        1, hex2dec('1a00'), hex2dec('3101'), 1;...  %  25
        1, hex2dec('1a01'), hex2dec('3101'), 2;...  %  26

        ];

%   Model       ProductCode          Revision          N, Rx, Tx
models = {...
  'EL2002', hex2dec('07d23052'), hex2dec('00100000'), 2, 1,  0; ...
  'EL2004', hex2dec('07d43052'), hex2dec('00100000'), 4, 1,  0; ...
  'EL2008', hex2dec('07d83052'), hex2dec('00100000'), 8, 1,  0; ...
  'EL2022', hex2dec('07e63052'), hex2dec('00100000'), 2, 1,  0; ...
  'EL2024', hex2dec('07e83052'), hex2dec('00100000'), 4, 1,  0; ...
  'EL2032', hex2dec('07f03052'), hex2dec('00100000'), 2, 1, 17; ...
  'EL2034', hex2dec('07f23052'), hex2dec('00100000'), 4, 1, 17; ...
  'EL2042', hex2dec('07fa3052'), hex2dec('00100000'), 2, 1,  0; ...
  'EL2084', hex2dec('08243052'), hex2dec('00100000'), 4, 1,  0; ...
  'EL2088', hex2dec('08283052'), hex2dec('00100000'), 8, 1,  0; ...
  'EL2124', hex2dec('084c3052'), hex2dec('00100000'), 4, 1,  0; ...
  'EL2202', hex2dec('089a3052'), hex2dec('00100000'), 2, 1,  0; ...
  'EL2602', hex2dec('0a2a3052'), hex2dec('00100000'), 2, 1,  0; ...
  'EL2612', hex2dec('0a343052'), hex2dec('00100000'), 2, 1,  0; ...
  'EL2622', hex2dec('0a3e3052'), hex2dec('00100000'), 2, 1,  0; ...
  'EL2624', hex2dec('0a403052'), hex2dec('00100000'), 4, 1,  0; ...
  'EL2712', hex2dec('0a983052'), hex2dec('00100000'), 2, 1,  0; ...
  'EL2722', hex2dec('0aa23052'), hex2dec('00100000'), 2, 1,  0; ...
  'EL2732', hex2dec('0aac3052'), hex2dec('00100000'), 2, 1,  0; ...
  'EL2798', hex2dec('0aee3052'), hex2dec('00100000'), 8, 1,  0; ...
  'EL2808', hex2dec('0af83052'), hex2dec('00100000'), 8, 1,  0; ...
  'EL2809', hex2dec('0af93052'), hex2dec('00100000'),16, 1,  0; ...
  'EL2004_00000000', ...
            hex2dec('07d43052'), hex2dec('00000000'), 4, 21, 0; ...
  'EL2032_00000000', ...
            hex2dec('07f03052'), hex2dec('00000000'), 2, 21,25; ...
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

% Find out which rows of pdo matrix we need
rows = product{5}:product{5}+product{4}-1;
if product{6}
    rows = [rows, product{6}:product{6}+product{4}-1];
end

% Populate the PDO structure
sm = unique(pdo(rows,1));
rv.sm = repmat({{0, 0, {}}}, 1, numel(sm));
for i = 1:numel(sm)
    rv.sm{i}{1} = sm(i);
    rv.sm{i}{3} = arrayfun(...
                @(x) {pdo(x,2), [pdo(x,3), pdo(x,4), 1]}, ...
                rows(pdo(rows,1) == sm(i)), ...
                'UniformOutput',0);
end

% if product{6} is set, the second SyncManager is a Tx SM
if numel(rv.sm) == 2
    rv.sm{2}{2} = double(product{6} ~= 0);
end
return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = port_config(SlaveConfig)
% Populate the blocks output port(s)

vector    = strcmp(get_param(gcbh,'vector'), 'on');
diag_port = strcmp(get_param(gcbh,'diag'),   'on');

sm_count  = sum(cellfun(@(x) (x{2} == 0), SlaveConfig.sm));
pdo_count = numel(SlaveConfig.sm{1}{3});
pdo_idx   = 0:pdo_count-1;
len = sm_count * pdo_count;
if (vector)
    rv.input.pdo      = zeros(len, 4);
    rv.input.pdo(:,1) = reshape(repmat(0:sm_count-1, pdo_count, 1),[],1);
    rv.input.pdo(:,2) = reshape(repmat(pdo_idx', 1, sm_count), [], 1);
    rv.input.pdo_data_type = 1001;
else
    rv.input  = repmat(struct('pdo',[0 0 0 0], 'pdo_data_type', 1001), 1, len);

    for i = 1:len
        rv.input(i).pdo(2)  = i-1;
    end
end

%[diag_port, numel(SlaveConfig.sm), SlaveConfig.sm{2}{2}, separate]
if diag_port && numel(SlaveConfig.sm) > 1 && SlaveConfig.sm{2}{2} == 1
    rv.output = rv.input;
    if (vector)
        rv.output.pdo(:,1) = ones(len, 1);
    else
        for i = 1:len
            rv.output(i).pdo(1) = 1;
        end
    end
end

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function update_gui(have_diag)
mask_enables = cell2struct(...
    get_param(gcbh,'MaskEnables'),...
    get_param(gcbh,'MaskNames')...
);

choice = {'off','on'};
value = choice{have_diag + 1};

if ~strcmp(mask_enables.diag,value)
    mask_enables.diag = value;
    set_param(gcbh,'MaskEnables',struct2cell(mask_enables));
end
