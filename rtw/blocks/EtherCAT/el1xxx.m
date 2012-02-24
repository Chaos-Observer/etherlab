function rv = el1xxx(model,output_type)

entries = [...
        hex2dec('3101'), 1, 1, 1001; ...
        hex2dec('3101'), 2, 1, 1001; ...
        hex2dec('3101'), 3, 1, 1001; ...
        hex2dec('3101'), 4, 1, 1001; ...

        hex2dec('6000'), 1, 1, 1001; ...
        hex2dec('6010'), 1, 1, 1001; ...
        hex2dec('6020'), 1, 1, 1001; ...
        hex2dec('6030'), 1, 1, 1001; ...
        hex2dec('6040'), 1, 1, 1001; ...
        hex2dec('6050'), 1, 1, 1001; ...
        hex2dec('6060'), 1, 1, 1001; ...
        hex2dec('6070'), 1, 1, 1001];

pdo = [...
        hex2dec('1a00'),  1, 1;...
        hex2dec('1a01'),  2, 2;...
        hex2dec('1a02'),  3, 3;...
        hex2dec('1a03'),  4, 4;...

        hex2dec('1a00'),  5, 5;...
        hex2dec('1a01'),  6, 6;...
        hex2dec('1a02'),  7, 7;...
        hex2dec('1a03'),  8, 8;...
        hex2dec('1a04'),  9, 9;...
        hex2dec('1a05'), 10, 10;...
        hex2dec('1a06'), 11, 11;...
        hex2dec('1a07'), 12, 12];

%   Model       ProductCode          Revision             PdoStartRow PdoCount
models = struct(...
    'EL1004'  ,[hex2dec('03ec3052'), hex2dec('0010000A'), 1,          4], ...
    'EL1014'  ,[hex2dec('03f63052'), hex2dec('00000000'), 1,          4], ...  
    'EL1004v2',[hex2dec('03ec3052'), hex2dec('00100000'), 5,          8], ...
    'EL1014v2',[hex2dec('03f63052'), hex2dec('00100000'), 5,          8], ...  
    'EL1034'  ,[hex2dec('040a3052'), hex2dec('00100000'), 5,          8], ...
    'EL1008'  ,[hex2dec('03f03052'), hex2dec('00100000'), 5,         12], ...
    'EL1018'  ,[hex2dec('03fa3052'), hex2dec('00100000'), 5,         12]  ...
    );

rv.SlaveConfig.vendor = 2;

rv.SlaveConfig.description = model;
product = models.(model);
rv.SlaveConfig.product = product(1);
rv.SlaveConfig.sm = {...
        {0, 1, {}} ...  % Only 1 output SyncManager
};

% Populate the PDO structure
rv.SlaveConfig.sm{1}{3} = arrayfun(...
        @(x) {pdo(x,1), entries(pdo(x,2):pdo(x,3),:)}, ...
        product(3):product(4), ...
        'UniformOutput',0 ...
);

% Populate the block's output port(s)
r = 0:(product(4) - product(3));
if strcmp(output_type, 'Vector Output')
    rv.PortConfig.output.pdo = zeros(numel(r),4);
    rv.PortConfig.output.pdo(:,2) = r;
else
    rv.PortConfig.output = arrayfun(@(x) struct('pdo', [0, x, 0, 0]), r);
end

return
