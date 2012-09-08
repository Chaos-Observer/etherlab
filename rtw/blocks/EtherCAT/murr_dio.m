function murr_dio(method, varargin)

if ~nargin
    return
end

%display([gcb ' ' method]);

switch lower(method)
case 'set'
    ud = get_param(gcbh,'UserData');
    model = get_param(gcbh, 'model');

    [ud.SlaveConfig, ud.PortConfig] = slave_config(model);
    set_param(gcbh, 'UserData', ud);

case 'check'
    % If UserData.SlaveConfig does not exist, this is an update
    % Convert this block and return
    model = get_param(gcbh,'model');

    ud = get_param(gcbh, 'UserData');

    % Get slave and port configuration based on product code and revision
    [sc,pc] = slave_config(ud.SlaveConfig.product, ud.SlaveConfig.revision);

    if isequal(sc.sm, ud.SlaveConfig.sm) && ~isequal(sc, ud.SlaveConfig)
        % The slave has a new name
        warning('murrimpact:NewName', ...
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
function [sc,pc] = slave_config(varargin)

sm.input  = struct('index', 2, 'pdo', [hex2dec('1600'), hex2dec('6200')]);
sm.output = struct('index', 3, 'pdo', [hex2dec('1a00'), hex2dec('6000')]);
sm.status = struct('index', 3, ...
                  'pdo', hex2dec('1a01'), 'entry', ...
                  {hex2dec('1001'), [1 3]; hex2dec('1002'), 1:8});

pdo = {2, hex2dec('1600'), [hex2dec('6199'), 8;
                            hex2dec('6200'), 1; 
                            hex2dec('6200'), 2; 
                            hex2dec('6200'), 3; 
                            hex2dec('6200'), 4; 
                            hex2dec('6200'), 5; 
                            hex2dec('6200'), 6; 
                            hex2dec('6200'), 7; 
                            hex2dec('6200'), 8;
                            hex2dec('6200'), 9; 
                            hex2dec('6200'), 10; 
                            hex2dec('6200'), 11; 
                            hex2dec('6200'), 12; 
                            hex2dec('6200'), 13; 
                            hex2dec('6200'), 14; 
                            hex2dec('6200'), 15; 
                            hex2dec('6200'), 16];

       3, hex2dec('1a00'), [hex2dec('5999'), 8;
                            hex2dec('6000'), 1;
                            hex2dec('6000'), 2;
                            hex2dec('6000'), 3;
                            hex2dec('6000'), 4;
                            hex2dec('6000'), 5;
                            hex2dec('6000'), 6;
                            hex2dec('6000'), 7;
                            hex2dec('6000'), 8;
                            hex2dec('6000'), 9;
                            hex2dec('6000'), 10;
                            hex2dec('6000'), 11;
                            hex2dec('6000'), 12;
                            hex2dec('6000'), 13;
                            hex2dec('6000'), 14;
                            hex2dec('6000'), 15;
                            hex2dec('6000'), 16];

       3, hex2dec('1a01'), [hex2dec('1001'), 1;
                            hex2dec('1001'), 2;
                            hex2dec('1001'), 3;
                            hex2dec('1001'), 4;
                            hex2dec('1001'), 5;
                            hex2dec('1001'), 6;
                            hex2dec('1001'), 7;
                            hex2dec('1001'), 8;
                            hex2dec('1002'), 1;
                            hex2dec('1002'), 2;
                            hex2dec('1002'), 3;
                            hex2dec('1002'), 4;
                            hex2dec('1002'), 5;
                            hex2dec('1002'), 6;
                            hex2dec('1002'), 7;
                            hex2dec('1002'), 8;
                            hex2dec('1002'), 9;
                            hex2dec('1002'), 10;
                            hex2dec('1002'), 11;
                            hex2dec('1002'), 12;
                            hex2dec('1002'), 13;
                            hex2dec('1002'), 14;
                            hex2dec('1002'), 15;
                            hex2dec('1002'), 16];
};

%   Model       ProductCode          Revision             PdoStartRow PdoEndRow
models = {...
    'DI8DO8', hex2dec('0000d72a'), hex2dec('00000001'),  {1, 2:9},  {2, 2:9},  {3, {1:4, 9:16}};
    'DO16',   hex2dec('0000d72c'), hex2dec('00000001'),  {1, 2:17}, {},        {3, {[1,2,4], 9:24}}; ...
    'DO8',    hex2dec('0000d72b'), hex2dec('00000001'),  {1, 2:9 }, {},        {3, {[1,2,4], 9:16}}; ...
    'DI16',   hex2dec('0000d729'), hex2dec('00000001'),  {},        {2, 2:17}, {3, {[1,3,4], []}}; ...
    };

sc = [];
pc = [];

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
    sc = vertcat(sort(fields(~obsolete)), sort(fields(obsolete)));
    return
end

if isempty(product)
    return;
end

sc.vendor = 79;
sc.description = product{1};
sc.product = product{2};
sc.revision = product{3};

vector  = strcmp(get_param(gcbh,'vector'), 'on');
%status  = strcmp(get_param(gcbh,'status'), 'on');

sc.sm = [];

% Input port (RxPdo)
if ~isempty(product{4})
    idx = product{4}{1};
    rows = product{4}{2};
    n = numel(rows);

    sc.sm{end+1} = {pdo{idx,1}, 0, { {pdo{idx,2}, horzcat(pdo{idx,3}(rows,:), repmat([1 1001], n, 1)) } }};
    if vector
        pc.input.pdo = repmat(0,n,4);
        pc.input.pdo(:,3) = 0:n-1;
    else
        pc.input = arrayfun(@(x) struct('pdo', [0 0 x 0]), 0:n-1);
    end
end

% Output port
if ~isempty(product{5})
    idx = product{5}{1};
    rows = product{5}{2};
    n = numel(rows);
    smidx = numel(sc.sm);

    sc.sm{end+1} = {pdo{idx,1}, 1, { {pdo{idx,2}, horzcat(pdo{idx,3}(rows,:), repmat([1 1001], n, 1)) } }};
    if vector
        pc.output.pdo = repmat([smidx,0,0,0],n,1);
        pc.output.pdo(:,3) = 0:n-1;
    else
        pc.output = arrayfun(@(x) struct('pdo', [smidx,0,x,0]), 0:n-1);
    end
end

% Status
if ~isempty(product{6})
    idx = product{6}{1};
    rows = product{6}{2};
    smidx = find(cellfun(@(x) x{1} == pdo{idx,1}, sc.sm));
    n = sum(cellfun(@(x) numel(x), rows));

    if isempty(smidx)
        smidx = numel(sc.sm)+1;
        sc.sm{end+1} = {pdo{idx,1}, 1, {}};

        pc.output = [];
    end

    pdoidx = numel(sc.sm{smidx}{3});
    sc.sm{smidx}{3}{end+1} = {pdo{idx,2}, horzcat(pdo{idx,3}(cell2mat(rows),:), repmat([1,1001], n, 1))};

    if vector
        x = 0:numel(rows{1})-1;
        pc.output(end+1).pdo = repmat([smidx-1,pdoidx,0,0], numel(x), 1);
        pc.output(end).pdo(:,3) = x;

        x = numel(x) + (0:numel(rows{2})-1);
        pc.output(end+1).pdo = repmat([smidx-1,pdoidx,0,0], numel(x), 1);
        pc.output(end).pdo(:,3) = x;
    else
        for i = 0:size(sc.sm{smidx}{3}{end}{2},1)-1
            pc.output(end+1).pdo = [smidx-1,pdoidx,i,0];
        end
    end
end

return
