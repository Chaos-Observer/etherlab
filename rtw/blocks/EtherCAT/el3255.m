%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Encapsulation for Potentiometer Slave EL3255
%
% Copyright (C) 2013 Richard Hacker
% License: LGPL
%
classdef el3255

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
    function rv = getSDO(obj,model)
        rv = obj.sdo;
    end

    %========================================================================
    function rv = configure(obj,model,count,vector,dc_spec,scaling,sdo)
        rv = [];

        row = find(strcmp(obj.models(:,1), model));

        % General information
        rv.SlaveConfig.vendor = 2;
        rv.SlaveConfig.product = obj.models{row,2};
        rv.SlaveConfig.description = obj.models{row,1};

        % Input syncmanager
        rv.SlaveConfig.sm = {{3,1,obj.pdo}};

        rv.SlaveConfig.sm{1}{3}(count+1:end) = [];

        range = (0:count-1)';
        if vector
            rv.PortConfig.output = struct(...
                'pdo', horzcat(zeros(count,1), range,zeros(count,2)),...
                'pdo_data_type', sint(16) ...
            );
            if count > 1
                rv.PortConfig.output.portname = ...
                        strcat('Ch. 1..',int2str(count));
            else
                rv.PortConfig.output.portname = strcat('Ch. 1');
            end
        else    % vector
            rv.PortConfig.output = struct(...
                'pdo', arrayfun(@(x) [0,x,0,0], range, ...
                                'UniformOutput', false), ...
                'pdo_data_type', sint(16), ...
                'portname', strcat({'Ch. '}, int2str(range+1)) ...
            );
        end     % vector

        % Distributed clocks
        if dc_spec(1)
            if dc_spec(1) ~= 4
                % DC Configuration from the default list
                rv.SlaveConfig.dc = obj.dc(dc_spec(1),:);
            else
                % Custom DC
                rv.SlaveConfig.dc = dc_spec(2:end);
            end
        end

        % Scaling and filter of output port 1
        if numel(scaling.gain) || numel(scaling.offset)
            rv.PortConfig.output(1).full_scale = 2^15;
        end
        if numel(scaling.gain)
            rv.PortConfig.output(1).gain = {'Gain', scaling.gain};
        end
        if numel(scaling.offset)
            rv.PortConfig.output(1).offset = {'Offset', scaling.offset};
        end
        if numel(scaling.filter)
            rv.PortConfig.output(1).filter = {'Filter', scaling.filter};
        end

        % SDO Configuration
        rv.SlaveConfig.sdo = num2cell(horzcat(obj.sdo, reshape(sdo,[],1)));
        rv.SlaveConfig.sdo(count+3:end,:) = [];

    end
end     % methods

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
properties (SetAccess=private)
    %  name          product code
    models = {
      'EL3255',      hex2dec('0cb73052');
    };

    % All known sdo's
    sdo = [hex2dec('8000'), hex2dec('06'),  8;
           hex2dec('8000'), hex2dec('15'),  8;
           hex2dec('8000'), hex2dec('1c'), 16;
           hex2dec('8010'), hex2dec('1c'), 16;
           hex2dec('8020'), hex2dec('1c'), 16;
           hex2dec('8030'), hex2dec('1c'), 16;
           hex2dec('8040'), hex2dec('1c'), 16];

    % PDO
    pdo = { {hex2dec('1a01'), [hex2dec('6000'), 17, 16]}, ...
            {hex2dec('1a03'), [hex2dec('6010'), 17, 16]}, ...
            {hex2dec('1a05'), [hex2dec('6020'), 17, 16]}, ...
            {hex2dec('1a07'), [hex2dec('6030'), 17, 16]}, ...
            {hex2dec('1a09'), [hex2dec('6040'), 17, 16]} };

    % Distributed Clock
    dc = [              0,0,0,0,0,0,0,0,    0,0;    % SM-Synchron
           hex2dec('700'),0,1,0,0,0,0,1,20000,0;    % DC-Synchron
           hex2dec('700'),0,1,0,0,1,0,1,10000,0];   % DC-Synchron (input based)
end     % properties

end     % classdef
