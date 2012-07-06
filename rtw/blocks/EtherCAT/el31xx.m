function rv = el31xx(model, output_type, status, dtype, scale, offset, ...
    filter, tau)

entries = [...
    hex2dec('3101'),  1,  8, 1008; ...
    hex2dec('3101'),  2, 16, 2016; ...
    hex2dec('3102'),  1,  8, 1008; ...
    hex2dec('3102'),  2, 16, 2016; ...
    hex2dec('6401'),  1, 16, 2016; ... % Channel outputs without states
    hex2dec('6401'),  2, 16, 2016; ... % Channel outputs without states

    hex2dec('6000'), 17, 16, 2016; ...
    hex2dec('6010'), 17, 16, 2016; ...
    hex2dec('6020'), 17, 16, 2016; ...
    hex2dec('6030'), 17, 16, 2016; ...
    ];

pdo = [...
    hex2dec('1a00'),  1,  2; ... % TxPdo Sm3 EL3102, EL3142, EL3152, EL3162
    hex2dec('1a01'),  3,  4; ... % TxPdo Sm3 EL3102, EL3142, EL3152, EL3162
    hex2dec('1a10'),  5,  6; ... % TxPdo Sm3 EL3102, EL3142, EL3152, EL3162

    hex2dec('1a01'),  7,  7; ...
    hex2dec('1a03'),  8,  8; ...
    hex2dec('1a05'),  9,  9; ...
    hex2dec('1a07'), 10, 10; ...
    ];

output_unit = struct (...
        'EL3102', '-10..10 V', ...
        'EL3142', '0..20 mA', ...
        'EL3152', '4..20 mA', ...
        'EL3162', '0..10 V', ...
        'EL3164', '0..10 V'...
        );

%
% Model ProductCode Revision | TxStart | TxEnd | TxStart | TxEnd
%                            | with status     | without status
%
models = struct( ...
    'EL3102', [hex2dec('0c1e3052'), hex2dec('00000000'), 1, 2, 3, 3], ...
    'EL3142', [hex2dec('0c463052'), hex2dec('00000000'), 1, 2, 3, 3], ...
    'EL3152', [hex2dec('0c503052'), hex2dec('00000000'), 1, 2, 3, 3], ...
    'EL3162', [hex2dec('0c5a3052'), hex2dec('00000000'), 1, 2, 3, 3], ...
    'EL3164', [hex2dec('0c5c3052'), hex2dec('00100000'), 0, 0, 4, 7] ...
    );

rv.output_unit = output_unit.(model);
rv.SlaveConfig.vendor = 2;
rv.SlaveConfig.description = model;

product = models.(model);
rv.SlaveConfig.product = product(1);
rv.SlaveConfig.revision = product(2);

% RxPdo SyncManager
rv.SlaveConfig.sm = { {3 1 {}} };

% Choose required pdos
if status
    pdoindex = product(3) : product(4);
else
    pdoindex = product(5) : product(6);
end

% Populate the RxPDO Inputs structure
rv.SlaveConfig.sm{1}{3} = arrayfun( ...
        @(x) {pdo(x, 1), entries(pdo(x, 2) : pdo(x, 3), :)}, ...
        pdoindex, ...
        'UniformOutput', 0 ...
);

% set scale for double outputs
scale_int = 2^15;

range = 1 : product(4) - product(3) + 1;
status_mapped = status && product(3);

% Set data type scale
if ~strcmp(dtype, 'Raw Bits')
end

% Fill in filter constants

filter_value = [];

is_scale_and_offset = strcmp(lower(dtype), 'double with scale and offset');
is_double = is_scale_and_offset || strcmp(lower(dtype), 'double');

% check parameter dimensions

if numel(tau) && numel(tau) ~= 1 && numel(tau) ~= numel(range)
    errordlg('Filter dimension invalid', gcb)
end

if is_scale_and_offset
    if numel(scale) && numel(scale) ~= 1 && numel(scale) ~= numel(range)
        errordlg('Scale dimension invalid', gcb)
    end
    if numel(offset) && numel(offset) ~= 1 && numel(offset) ~= numel(range)
        errordlg('Offset dimension invalid', gcb)
    end
end

% Populate the block's output port(s)

% pdo indices
if numel(rv.SlaveConfig.sm{1}{3}) > 1
    pdo_idx = range - 1;
    entry_idx = zeros(numel(range), 1);
else
    pdo_idx = zeros(numel(range), 1);
    entry_idx = range - 1;
end

if strcmp(output_type, 'Vector Output')

    % value port
    rv.PortConfig.output.pdo = zeros(numel(range), 4);
    rv.PortConfig.output.pdo(:, 2) = pdo_idx;
    rv.PortConfig.output.pdo(:, 3) = entry_idx + status_mapped;

    if is_double
        rv.PortConfig.output.full_scale = scale_int;

        if ~isempty(tau)
            rv.PortConfig.output.filter = {'Filter', tau};
        end
    end

    if is_scale_and_offset
        if ~isempty(scale)
            rv.PortConfig.output.gain = {'Gain', scale};
        end
        if ~isempty(offset)
            rv.PortConfig.output.offset = {'Offset', offset};
        end
    end

    if status_mapped
        % status port
        rv.PortConfig.output(2).pdo = zeros(numel(range), 4);
        rv.PortConfig.output(2).pdo(:, 2) = pdo_idx;
        rv.PortConfig.output(2).pdo(:, 3) = entry_idx;
    end
else
    rv.PortConfig.output = [];
    for k = range
        p = numel(rv.PortConfig.output) + 1;
        rv.PortConfig.output(p).pdo = ...
            [0, pdo_idx(k), entry_idx(k) + status_mapped, 0];

        if is_double
            rv.PortConfig.output(p).full_scale = scale_int;

            filter_name = ['Filter', num2str(k)];
            if numel(tau) > 1
                rv.PortConfig.output(p).filter = {filter_name, tau(k)};
            elseif numel(tau) == 1
                rv.PortConfig.output(p).filter = {filter_name, tau};
            end
        end

        if is_scale_and_offset
            if ~isempty(scale)
                rv.PortConfig.output(p).gain = {'Gain', scale};
            end
            if ~isempty(offset)
                rv.PortConfig.output(p).offset = {'Offset', offset};
            end
        end

        if status_mapped
            rv.PortConfig.output(p+1).pdo = [0, pdo_idx(k), entry_idx(k), 0];
        end
    end
end
