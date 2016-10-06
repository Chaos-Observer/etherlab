%
% Master 0, Slave 58, "EK1914"
%
function rv = ek1914(stdio, safeio)

% Slave configuration

rv.SlaveConfig.vendor = 2;
rv.SlaveConfig.product = hex2dec('077a2c52');
rv.SlaveConfig.description = 'EK1914';
rv.SlaveConfig.sm = {
    {2, 0, {
        {hex2dec('1600'), [
            hex2dec('7000'), hex2dec('01'),   8;
            hex2dec('7001'), hex2dec('01'),   1;
            hex2dec('7001'), hex2dec('02'),   1;
            hex2dec('0000'), hex2dec('00'),   6;
            hex2dec('7000'), hex2dec('03'),  16;
            hex2dec('7000'), hex2dec('02'),  16;
            ]},
        {hex2dec('1601'), [
            hex2dec('7010'), hex2dec('01'),   1;
            hex2dec('7010'), hex2dec('02'),   1;
            hex2dec('7010'), hex2dec('03'),   1;
            hex2dec('7010'), hex2dec('04'),   1;
            hex2dec('7010'), hex2dec('05'),   1;
            hex2dec('7010'), hex2dec('06'),   1;
            hex2dec('0000'), hex2dec('00'),  10;
            ]},
        }},
    {3, 1, {
        {hex2dec('1a00'), [
            hex2dec('6000'), hex2dec('01'),   8;
            hex2dec('6001'), hex2dec('01'),   1;
            hex2dec('6001'), hex2dec('02'),   1;
            hex2dec('0000'), hex2dec('00'),   6;
            hex2dec('6000'), hex2dec('03'),  16;
            hex2dec('6000'), hex2dec('02'),  16;
            ]},
        {hex2dec('1a01'), [
            hex2dec('6010'), hex2dec('01'),   1;
            hex2dec('6010'), hex2dec('02'),   1;
            hex2dec('6010'), hex2dec('03'),   1;
            hex2dec('6010'), hex2dec('04'),   1;
            hex2dec('0000'), hex2dec('00'),  12;
            ]},
        }}
    };

% FSoE IO
rv.PortConfig.input.pdo = [0, 0];
rv.PortConfig.input.pdo_data_type = [];
rv.PortConfig.input.portname = 'FSoE';

rv.PortConfig.output.pdo = [1, 0];
rv.PortConfig.output.pdo_data_type = [];
rv.PortConfig.output.portname = 'FSoE';

if safeio
    rv.PortConfig.input(end+1) = struct(...
        'pdo',              [0,1,4,0; 0,1,5,0], ...
        'pdo_data_type',    uint(1), ...
        'portname',         'SafeOut');

    rv.PortConfig.output(end+1) = struct(...
        'pdo',              [1,0,1,0; 1,0,2,0], ...
        'pdo_data_type',    uint(1), ...
        'portname',         'SafeIn');
end

if stdio
    rv.PortConfig.output(end+1) = struct(...
        'pdo', [ones(4,2), (0:3)', zeros(4,1)], ...
        'pdo_data_type', uint(1), ...
        'portname', 'StdIn');

    rv.PortConfig.input(end+1) = struct(...
        'pdo', [repmat([0,1],4,1), (0:3)', zeros(4,1)], ...
        'pdo_data_type', uint(1), ...
        'portname', 'StdOut');
end
