classdef EtherCATSlaveBlock
%% Class to mask an EtherCAT Slave block
% This class provides methods to manage a slave
%
% Methods:
%       EtherCATSlaveBlock(b)
%       updateModels(obj,models)
%       updateCustomDCEnable(obj)
%       updateDCVisibility(obj,state)
%       updateSDOVisibility(obj,sdo)

properties (Access = private)
    block
    maskNames
end

methods
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function obj = EtherCATSlaveBlock(b)
        if nargin > 0
            obj.block = b;
        else
            obj.block = gcbh;
        end
        obj.maskNames = get_param(obj.block,'MaskNames');
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateModels(obj,slave)
        %% Keeps the slave model list up to date
        % Arguments
        %       slave: cell array where first column is the list of models
        %
        % Requires the following mask variables:
        %       model

        % Trim the models
        if isa(slave,'cell')
            models = slave;
        else
            models = slave.getModels();
        end
        models = models(:,1);

        if verLessThan('Simulink','8.0')
            style = get_param(obj.block,'MaskStyles');

            pos = find(strcmp(obj.maskNames,'model'));

            modelstr = ['popup(', EtherCATSlaveBlock.strJoin(models,'|'), ')'];
            if ~strcmp(style{pos}, modelstr)
                style{pos} = modelstr;
                set_param(obj.block,'MaskStyles', style);
                display(['Updated ', obj.block])
            end
        else
            p = Simulink.Mask.get(obj.block);
            pos = find(strcmp({p.Parameters.Name}, 'model'), 1);
            if ~isequal(p.Parameters(pos).TypeOptions, models)
                p.Parameters(pos).set('TypeOptions', models(:,1));
                display(['Updated ', obj.block])
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function setPortNames(obj,input,output,deflt)
        fmt = 'port_label(''%s'', %i, ''%s'')\n';

        if nargin > 3
            str = {sprintf('disp(''%s'')\n',deflt)};
        else
            str = {''};
        end

        str = horzcat(str, ...
                arrayfun(@(x) sprintf(fmt, 'input', x, input{x}), ...
                         1:numel(input), 'UniformOutput', false), ...
                arrayfun(@(x) sprintf(fmt, 'output', x, output{x}), ...
                         1:numel(output), 'UniformOutput', false));

        str = cell2mat(str);

        set_param(obj.block, 'MaskDisplay', str);
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateCustomDCEnable(obj)
        % This function sets the enable state of all mask variables
        % where custom values for distributed clock can be entered.
        % if dc_mode == 'Custom' they are enabled
        %
        % Requires the following mask varialbes:
        %       dc_mode, dc_*

        mode_idx = find(strcmp(obj.maskNames,'dc_mode'));
        n = setdiff(find(strncmp(obj.maskNames,'dc_',3)), mode_idx);

        if strcmp(get_param(obj.block,'dc_mode'),'Custom')
            setEnable(obj, obj.maskNames(n), true);
        else
            setEnable(obj, obj.maskNames(n), false);
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateDCVisibility(obj,state)
        %% Change the visibility of all mask variables
        % starting with 'dc_' depending on <state>
        %
        % Requires the following mask varialbes:
        %       dc_mode, dc_*

        n = find(strncmp(obj.maskNames,'dc_',3));
        setVisible(obj, obj.maskNames(n), state)
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateSDOVisibility(obj,sdoList)
        %% Change visibility of SDO Mask Variables
        % SDO Variables start with 'x8'
        % Argument:
        %       sdoList: array of vars [Index, SubIndex]
        %
        % Requires the following mask varialbes:
        %       x8*

        sdo = obj.maskNames(strncmp(obj.maskNames,'x8',2));
        enable = cellstr(strcat('x', dec2base(sdoList(:,1), 16, 4),...
                                '_', dec2base(sdoList(:,2), 16, 2)));

        state = repmat(0, size(sdo));
        state(cellfun(@(i) find(strcmp(sdo,i)), enable)) = 1;

        setEnable(obj,sdo,state);
    end


    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function setEnable(obj, var, state)
        %% Change enabled state of a mask variable
        % Arguments:
        %   var: string or cellarray of strings
        %   state: boolean

        if isstr(var)
            var = {var};
        end

        vis = get_param(obj.block,'MaskEnables');
        n = cellfun(@(i) find(strcmp(obj.maskNames,i)), var);

        vis(n) = EtherCATSlaveBlock.enableSet(state);

        if ~isequal(get_param(obj.block,'MaskEnables'),vis)
            set_param(obj.block,'MaskEnables',vis);
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function setVisible(obj, var, state)
        %% Change visibility of a mask variable
        % Arguments:
        %   var: string or cellarray of strings
        %   state: boolean

        if isstr(var)
            var = {var};
        end

        vis = get_param(obj.block,'MaskVisibilities');
        n = cellfun(@(i) find(strcmp(obj.maskNames,i)), var);

        vis(n) = EtherCATSlaveBlock.enableSet(state);

        if ~isequal(get_param(obj.block,'MaskVisibilities'),vis)
            set_param(obj.block,'MaskVisibilities',vis);
        end
    end

end

methods (Static)
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function s = enableSet(state)
        %% returns a cellarray with elements 'on' or 'off' depending on state
        s = {'off','on'};
        s = s((state ~= 0) + 1);
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function rv = strJoin(s,delim)
        rv = cell2mat(strcat(reshape(s,1,[]),delim));
        rv(end) = [];
    end
end

end     % classdef
