classdef EtherCATSlaveBlock
%% Class to mask an EtherCAT Slave block
% This class provides methods to manage a slave
%
% Methods:
%    function rv = formatAddress(master,index,tsample)
%    function updateSlaveState(slave)
%    function updateModels(list)
%    function setPortNames(input,output,deflt)
%    function updateCustomDCEnable()
%    function updateDCVisibility(state)
%    function updateSDOVisibility(sdoList)
%    function setEnable(var, state)
%    function setVisible(var, state)
%    function s = enableSet(state)
%    function rv = strJoin(s,delim)

methods (Static)
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updatePdoVisibility(slave,pdo,state)
        model = get_param(gcbh,'model');
        exclude = slave.getExcludeList(model, hex2dec(pdo));
        if isempty(exclude)
            return
        end
        pdo = cellstr(strcat('pdo_x',dec2hex(exclude)));
        EtherCATSlaveBlock.setEnable(pdo, state);
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateSlaveState(slave)
        %% This method filters out all PDO mask entries (everything starting
        % with 'x1'). It then queries the slave for 

        model = get_param(gcbh,'model');
        names = get_param(gcbh,'MaskNames');

        % The names of PDO's
        pdoNames = names(strncmp(names, 'pdo_x', 5));

        % and a list of index numbers they represent
        pdo = cell2mat(pdoNames);
        pdo = hex2dec(pdo(:,6:end));

        % A list of checked PDO's
        checkedPdo = pdo(cellfun(@(x) strcmp(get_param(gcbh,x), 'on'),...
                                 pdoNames));

        % Ask the slave which PDO's must be disabled
        exclude = slave.getExcludeList(model, checkedPdo);
        EtherCATSlaveBlock.setEnable(pdoNames(ismember(pdo,exclude)), false);
        EtherCATSlaveBlock.setEnable(pdoNames(~ismember(pdo,exclude)), true);

        % Now check the visibility of the PDO's
        visible = slave.pdoVisible(model);
        EtherCATSlaveBlock.setVisible(pdoNames(~ismember(pdo,visible)),false);
        EtherCATSlaveBlock.setVisible(pdoNames( ismember(pdo,visible)), true);

        % Now check the visibility of the SDO's
        sdoNames = names(strncmp(names,'sdo_',4));
        x = cellstr(strcat('sdo_',dec2base(slave.getSDO(model),10,2)))';
        EtherCATSlaveBlock.setVisible(sdoNames( ismember(sdoNames,x)), true);
        EtherCATSlaveBlock.setVisible(sdoNames(~ismember(sdoNames,x)),false);
    end

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
        model = get_param(gcbh,'model');
        names = get_param(gcbh,'MaskNames');

        % This function sets the enable state of all mask variables
        % where custom values for distributed clock can be entered.
        % if dc_mode == 'Custom' they are enabled
        %
        % Requires the following mask varialbes:
        %       dc_mode, dc_*

        mode_idx = find(strcmp(names,'dc_mode'));
        n = setdiff(find(strncmp(names,'dc_',3)), mode_idx);

        if strcmp(get_param(gcbh,'dc_mode'),'Custom')
            EtherCATSlaveBlock.setEnable(names(n), true);
        else
            EtherCATSlaveBlock.setEnable(names(n), false);
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateDCVisibility(state)
        %% Change the visibility of all mask variables
        % starting with 'dc_' depending on <state>
        %
        % Requires the following mask varialbes:
        %       dc_mode, dc_*

        names = get_param(gcbh,'MaskNames');
        EtherCATSlaveBlock.setVisible(names(strncmp(names,'dc_',3)), state)
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateSDOVisibility(sdoList)
        %% Change visibility of SDO Mask Variables
        % SDO Variables start with 'sdo_'
        % Argument:
        %       sdoList: array of vars

        names = get_param(gcbh,'MaskNames');
        sdo = names(strncmp(names,'sdo_',4));
        enable = cellstr(strcat('sdo_',dec2base(sdoList,10,2)))';

        state = repmat(0, size(sdo));
        state(cellfun(@(i) find(strcmp(sdo,i)), enable)) = 1;

        EtherCATSlaveBlock.setEnable(sdo,state);
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function setEnable(var, state)
        %% Change enabled state of a mask variable
        % Arguments:
        %   var: string or cellarray of strings
        %   state: boolean

        if isempty(var)
            return
        end

        maskNames = get_param(gcbh,'MaskNames');

        if isstr(var)
            var = {var};
        end

        vis = get_param(gcbh,'MaskEnables');
        n = cellfun(@(i) find(strcmp(maskNames,i)), var);

        vis(n) = EtherCATSlaveBlock.enableSet(state);

        if ~isequal(get_param(gcbh,'MaskEnables'),vis)
            set_param(gcbh,'MaskEnables',vis);
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function setVisible(var, state)
        %% Change visibility of a mask variable
        % Arguments:
        %   var: string or cellarray of strings
        %   state: boolean

        if isempty(var)
            return
        end

        maskNames = get_param(gcbh,'MaskNames');

        if isstr(var)
            var = {var};
        end

        vis = get_param(gcbh,'MaskVisibilities');
        n = cellfun(@(i) find(strcmp(maskNames,i)), var);

        vis(n) = EtherCATSlaveBlock.enableSet(state);

        if ~isequal(get_param(gcbh,'MaskVisibilities'),vis)
            set_param(gcbh,'MaskVisibilities',vis);
        end
    end

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
