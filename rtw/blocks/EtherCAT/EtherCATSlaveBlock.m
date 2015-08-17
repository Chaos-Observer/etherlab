classdef EtherCATSlaveBlock
%% Class to mask an EtherCAT Slave block
% This class provides methods to manage a slave
%
% Methods:
%    function updateModels(list)
%    function setPortNames(input,output,deflt)
%    function updateCustomDCEnable()
%    function updateDCVisibility(state)
%    function updatePDOVisibility(list)
%    function updateSDOVisibility(sdoList)
%    function updateSDOEnable(sdoList)
%    function setEnable(list, on, off)
%    function setVisible(list, on, off)

methods (Static)

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateModels(list)
        %% Keeps the list model list up to date
        % Arguments
        %       list: cell array where first column is the list of models
        %
        % Requires the following mask variables:
        %       model

        % Trim the models
        if ~iscell(list)
            disp('First parameter must be a cell array')
            return
        end
        models = list(:,1);

        if verLessThan('Simulink','8.0')
            style = get_param(gcbh,'MaskStyles');
            names = get_param(gcbh,'MaskNames');

            pos = find(strcmp(names,'model'));

            modelstr = ['popup(', EtherCATSlaveBlock.strJoin(models,'|'), ')'];
            if ~strcmp(style{pos}, modelstr)
                style{pos} = modelstr;
                set_param(gcbh,'MaskStyles', style);
                display(['Updated ', gcb])
            end
        else
            p = Simulink.Mask.get(gcbh);
            pos = find(strcmp({p.Parameters.Name}, 'model'), 1);
            if ~isequal(p.Parameters(pos).TypeOptions, models)
                p.Parameters(pos).set('TypeOptions', models(:,1));
                display(['Updated ', gcb])
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function checkFilter(omega)
        if ~isempty(omega)
            disp(sprintf(['%s: Deprecated use of LPF Frequency dialog parameter. ' ...
                 'See <matlab:web(etherlab_help_path(''general.html#filter''), ''-helpbrowser'')>'], ...
                gcb))
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function setPortNames(input,output,deflt)
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

        set_param(gcbh, 'MaskDisplay', str);
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateCustomDCEnable()
        names = get_param(gcbh,'MaskNames');

        % This function sets the enable state of all mask variables
        % where custom values for distributed clock can be entered.
        % if dc_mode == 'Custom' they are enabled
        %
        % Requires the following mask variables:
        %       dc_mode, dc_*

        dc = strncmp(names,'dc_',3);

        % Mask out dc_mode itself
        dc(strcmp(names,'dc_mode')) = false;

        EtherCATSlaveBlock.setEnable(dc, ...
                strcmp(get_param(gcbh,'dc_mode'),'Custom'));
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateDCVisibility(state)
        %% Change the visibility (on/off) of all mask variables
        % starting with 'dc_' depending on <state>
        %
        % Requires the following mask varialbes:
        %       dc_mode, dc_*

        dc = strncmp(get_param(gcbh,'MaskNames'),'dc_',3);

        EtherCATSlaveBlock.setVisible(dc, state);
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updatePDOVisibility(list)
        %% Make mask variables of type pdo_xXXXX visible
        %    list       - double vector of indices
        [idx,enable] = EtherCATSlaveBlock.getVariableList('pdo_x',...
                                                          dec2hex(list,4));
        EtherCATSlaveBlock.setVisible(idx,enable)
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateSDOVisibility(sdoList)
        %% Change visibility (on/off) of SDO Mask Variables
        % SDO Variables start with 'sdo_'
        % Argument:
        %       sdoList: (cellarray of strings|string array)
        %                eg: cellstr(dec2hex(1:20,2));
        [idx,enable] = EtherCATSlaveBlock.getVariableList('sdo_', sdoList);
        EtherCATSlaveBlock.setVisible(idx,enable)
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateSDOEnable(sdoList)
        %% Change enable state (gray/black) of SDO Mask Variables
        % SDO Variables start with 'sdo_'
        % Argument:
        %       sdoList: (cellarray of strings|string array)
        %                eg: cellstr(dec2hex(1:20,2));
        [idx,enable] = EtherCATSlaveBlock.getVariableList('sdo_', sdoList);
        EtherCATSlaveBlock.setEnable(idx, enable);
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function setEnable(list, state)
        %% Change enabled state of a mask variable
        % Arguments:
        %   state/off: cellarray of strings or logical array
        %           to enable/disable a mask variable

        EtherCATSlaveBlock.setMaskState('MaskEnables', list, state);
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function setVisible(list, state)
        %% Change visibility of a mask variable
        % Arguments:
        %   state/off: cellarray of strings or logical array
        %           to make visible/invisible

        EtherCATSlaveBlock.setMaskState('MaskVisibilities', list, state);
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function saveModelRevision(slave)
        revision = slave.getModelRevision();
        set_param(gcbh,'revision',mat2str(revision));
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function checkModelRevision(slave)
        if isfield(get_param(gcbh, 'DialogParameters'), 'revision')
            r = get_param(gcbh,'revision')
            class(r)
            revision = eval('[]'); %get_param(gcbh,'revision'));
        else
            revision = '';
        end

        model = slave.getOldModel(revision);

        if ~strcmp(model, get_param(gcbh, 'model'))
            set_param(gcbh, 'model', model)
        end
    end

end

methods (Static, Access = private)

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function [idx,enable] = getVariableList(prefix, enableList)
        %% Return a list of indices for all mask names that
        % start with prefix. enable is also set when a variable is
        % in the enableList

        % Get a list of names that begin with prefix
        names = get_param(gcbh,'MaskNames');
        idx = strncmp(names,prefix,length(prefix));

        enable = ismember(names, strcat(prefix, enableList));
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function setMaskState(variable, list, on)
        if numel(on) == 1
            on = repmat(on, size(list));
        end

        if ~islogical(list)
            names = get_param(gcbh, 'MaskNames');
            [list, pos] = ismember(names, list);
            i = on;
            on = list;
            on(list) = i(pos(list));
        end

        state = get_param(gcbh,variable);

        state(list &  on) = {'on'};
        state(list & ~on) = {'off'};

        if ~isequal(get_param(gcbh,variable),state)
            set_param(gcbh,variable,state);
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function rv = strJoin(s,delim)
        rv = cell2mat(strcat(reshape(s,1,[]),delim));
        rv(end) = [];
    end
end     % methods

end     % classdef
