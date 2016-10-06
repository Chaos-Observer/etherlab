function rv = el2904(safein)

% -300SO12 (EL2904)
rv.SlaveConfig.vendor = 2;
rv.SlaveConfig.product = 190328914;
rv.SlaveConfig.description = 'EL2904';
rv.SlaveConfig.sm = {
    {2, 0, {
        {hex2dec('1600'), [
            hex2dec('7000'), hex2dec('01'),   8;
            hex2dec('7001'), hex2dec('01'),   1;
            hex2dec('7001'), hex2dec('02'),   1;
            hex2dec('7001'), hex2dec('03'),   1;
            hex2dec('7001'), hex2dec('04'),   1;
            hex2dec('0000'), hex2dec('00'),   4;
            hex2dec('7000'), hex2dec('02'),  16;
            hex2dec('7000'), hex2dec('03'),  16;
            ]},
        {hex2dec('1601'), [
            hex2dec('7010'), hex2dec('01'),   1;
            hex2dec('7010'), hex2dec('02'),   1;
            hex2dec('7010'), hex2dec('03'),   1;
            hex2dec('7010'), hex2dec('04'),   1;
            hex2dec('0000'), hex2dec('00'),  12;
            ]},
        }},
    {3, 1, {
        {hex2dec('1a00'), [
            hex2dec('6000'), hex2dec('01'),   8;
            hex2dec('0000'), hex2dec('00'),   8;
            hex2dec('6000'), hex2dec('03'),  16;
            hex2dec('6000'), hex2dec('04'),  16;
            ]},
        }},
};

% FSoE IO
rv.PortConfig.input.pdo = [0,0];
rv.PortConfig.input.portname = 'FSoE';

rv.PortConfig.output.pdo = [1,0];
rv.PortConfig.output.portname = 'FSoE';

if safein
    rv.PortConfig.input(2).pdo = [repmat([0,1],4,1), (0:3)', zeros(4,1)];
    rv.PortConfig.input(2).pdo_data_type = uint(1);
    rv.PortConfig.input(2).portname = 'SafeOut';
end
