%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Encapsulation for Sin/Cos Encoder EL5021
%
% Copyright (C) 2013 Richard Hacker
% License: LGPL
%
classdef el5021

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
methods
    %========================================================================
    function rv = getModels(obj)
        rv = obj.models(:,1);
    end

    %========================================================================
    function rv = getDC(obj,model)
        rv = obj.dc;
    end

    %========================================================================
    function rv = configure(obj,model,dc_spec,sdo)
        rv = [];

        row = find(strcmp(obj.models(:,1), model));

        % General information
        rv.SlaveConfig.vendor = 2;
        rv.SlaveConfig.product = obj.models{row,2};
        rv.SlaveConfig.description = obj.models{row,1};

        % Input syncmanager
        rv.SlaveConfig.sm = {{2, 0, {{obj.pdo{1}{1} []}}},
                             {3, 1, {{obj.pdo{2}{1} []}}}};
        rv.SlaveConfig.sm{1}{3}{1}{2} = cell2mat(obj.pdo{1}{2}(:,1:3));
        rv.SlaveConfig.sm{2}{3}{1}{2} = cell2mat(obj.pdo{2}{2}(:,1:3));

        % CoE Configuration
        rv.SlaveConfig.sdo = num2cell(horzcat(obj.sdo,sdo'));

        % Distributed clocks
        if dc_spec(1) ~= 4
            % DC Configuration from the default list
            rv.SlaveConfig.dc = obj.dc(dc_spec(1),:);
        else
            % Custom DC
            rv.SlaveConfig.dc = dc_spec(2:end);
        end

        % Input port
        rv.PortConfig.input(1).pdo = [0,0,0,0; 0,0,2,0];
        rv.PortConfig.input(1).pdo_data_type = uint(1);
        rv.PortConfig.input(2).pdo = [0,0,4,0];
        rv.PortConfig.input(2).pdo_data_type = uint(32);

        rv.PortConfig.output(1).pdo = repmat([1,0,0,0], 5, 1);
        rv.PortConfig.output(1).pdo(:,3) = [0,2,3,4,6];
        rv.PortConfig.output(1).pdo_data_type = uint(1);
        rv.PortConfig.output(2).pdo = [1,0,11,0];
        rv.PortConfig.output(2).pdo_data_type = uint(32);
        rv.PortConfig.output(3).pdo = [1,0,12,0];
        rv.PortConfig.output(3).pdo_data_type = uint(32);

    end
end     % methods

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
properties (SetAccess=private)
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
