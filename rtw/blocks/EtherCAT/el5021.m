%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Encapsulation for Sin/Cos Encoder EL5021
%
% Copyright (C) 2013 Richard Hacker
% License: LGPL
%
classdef el5021 < EtherCATSlave

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
methods (Static)
    %========================================================================
    function rv = configure(model,dc_spec,sdo)
        slave = EtherCATSlave.findSlave(model,el5021.models);

        % General information
        rv.SlaveConfig.vendor = 2;
        rv.SlaveConfig.product = slave{2};
        rv.SlaveConfig.description = slave{1};

        pdo_list = el5021.pdo;

        % Input syncmanager
        rv.SlaveConfig.sm = {{2, 0, {{pdo_list{1}{1} []}}},
                             {3, 1, {{pdo_list{2}{1} []}}}};
        rv.SlaveConfig.sm{1}{3}{1}{2} = cell2mat(pdo_list{1}{2}(:,1:3));
        rv.SlaveConfig.sm{2}{3}{1}{2} = cell2mat(pdo_list{2}{2}(:,1:3));

        % CoE Configuration
        rv.SlaveConfig.sdo = num2cell(horzcat(el5021.sdo,sdo'));

        % Distributed clocks
        if dc_spec(1) ~= 4
            % DC Configuration from the default list
            dc = el5021.dc;
            rv.SlaveConfig.dc = dc(dc_spec(1),:);
        else
            % Custom DC
            rv.SlaveConfig.dc = dc_spec(2:end);
        end

        % Input port
        rv.PortConfig.input(1).pdo = [0,0,0,0; 0,0,2,0];
        rv.PortConfig.input(1).pdo_data_type = uint(1);
        rv.PortConfig.input(1).portname = 'bool[2]';
        rv.PortConfig.input(2).pdo = [0,0,4,0];
        rv.PortConfig.input(2).pdo_data_type = uint(32);
        rv.PortConfig.input(2).portname = 'Value';

        rv.PortConfig.output(1).pdo = repmat([1,0,0,0], 5, 1);
        rv.PortConfig.output(1).pdo(:,3) = [0,2,3,4,6];
        rv.PortConfig.output(1).pdo_data_type = uint(1);
        rv.PortConfig.output(1).portname = 'bool[5]';
        rv.PortConfig.output(2).pdo = [1,0,11,0];
        rv.PortConfig.output(2).pdo_data_type = uint(32);
        rv.PortConfig.output(2).portname = 'Counter';
        rv.PortConfig.output(3).pdo = [1,0,12,0];
        rv.PortConfig.output(3).pdo_data_type = uint(32);
        rv.PortConfig.output(3).portname = 'Latch';

    end

    %====================================================================
    function test(p)
        ei = EtherCATInfo(fullfile(p,'Beckhoff EL5xxx.xml'));
        for i = 1:size(el5021.models,1)
            fprintf('Testing %s\n', el5021.models{i,1});
            rv = el5021.configure(el5021.models{i,1},2,1:5);
            ei.testConfiguration(rv.SlaveConfig,rv.PortConfig);
        end
    end
end     % methods

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
properties (Constant)
    %  name          product code
    models = {...
      'EL5021',      hex2dec('139d3052');
    };

    % PDO class
    pdo = { {hex2dec('1600'), {hex2dec('7000'),  1,  1
                               hex2dec('0'),     0,  1
                               hex2dec('7000'),  3,  1
                               hex2dec('0'),     0, 13
                               hex2dec('7000'), 17, 32}},
            {hex2dec('1a00'), {hex2dec('6000'),  1,  1
                               hex2dec('0'),     0,  1
                               hex2dec('6000'),  3,  1
                               hex2dec('6001'),  4,  1
                               hex2dec('6001'),  5,  1
                               hex2dec('0'),     0,  5
                               hex2dec('6000'), 11,  1
                               hex2dec('0'),     0,  2
                               hex2dec('1c32'), 32,  1
                               hex2dec('1800'),  7,  1
                               hex2dec('1800'),  9,  1
                               hex2dec('6000'), 17, 32
                               hex2dec('6000'), 18, 32}}};

    sdo = [hex2dec('8000'), hex2dec('01'),   8;
           hex2dec('8000'), hex2dec('0E'),   8;
           hex2dec('8001'), hex2dec('01'),   8;
           hex2dec('8001'), hex2dec('02'),   8;
           hex2dec('8001'), hex2dec('11'),   8;];

    dc = [              0,0,0,     0,0,0,0,0,    0,0;    % SM-Synchron
           hex2dec('700'),0,1,-30600,0,0,0,1,25000,0;    % DC-Synchron
           hex2dec('700'),0,1,-30600,0,1,0,1,25000,0];   % DC-Synchron (input based)
end     % properties

end     % classdef
