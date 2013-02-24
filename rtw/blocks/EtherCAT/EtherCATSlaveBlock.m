classdef EtherCATSlaveBlock
%% Class to mask an EtherCAT Slave block
% This class provides methods to manage a slave
%
% Methods:
%       EtherCATSlaveBlock(b)
%       updateModels(obj,models)
%       checkProductCodeAndRevision(obj,models)
%       updateCustomDCEnable(obj)
%       updateDCVisibility(obj,state)
%       updateSDOVisibility(obj,sdo)

properties (SetAccess = private)
    block = gcb;
    maskNames
end

methods
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function obj = EtherCATSlaveBlock(b)
        if nargin > 0
            obj.block = b;
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
        models = slave.getModels();
        models = models(:,1);

        if verLessThan('Simulink','8.0')
            style = get_param(obj.block,'MaskStyles');

            pos = find(strcmp(obj.maskNames,'model'));

            modelstr = ['popup(', strJoin(models,'|'), ')'];
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
    function setPortNames(obj,input,output)
        fmt = 'port_label(''%s'', %i, ''%s'')\n';
        str = horzcat(...
                arrayfun(@(x) sprintf(fmt, 'input', x, input{x}), ...
                         1:numel(input), 'UniformOutput', false), ...
                arrayfun(@(x) sprintf(fmt, 'output', x, output{x}), ...
                         1:numel(output), 'UniformOutput', false));

        set_param(obj.block, 'MaskDisplay', cell2mat(str));
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function setProductCodeAndRevision(obj,codes)
        % Make sure that the product code and revision is ok and matches model
        % Arguments
        %       codes: [ProductCode, RevisionNo]
        %
        % Requires the following mask variables:
        %       product_code, revision_number

        set_param(obj.block, 'product_code',    int2str(codes(1)));
        set_param(obj.block, 'revision_number', int2str(codes(2)));
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function checkProductCodeAndRevision(obj,slave)
        % Make sure that the product code and revision is ok and matches model
        % Arguments
        %       slave: EtherCAT Slave object
        %
        % Requires the following mask variables:
        %       product_code, revision_number, model

        models = slave.getModels();

        pc       = str2double(get_param(obj.block, 'product_code'));
        revision = str2double(get_param(obj.block, 'revision_number'));
        row = find(cell2mat(models(:,2)) == pc ...
                    & cell2mat(models(:,3)) == revision, 1);
    
        selected_model = get_param(obj.block,'model');
    
        if ~isempty(row) && strcmp(selected_model, models(row,1))
            % Nothing changed
            return
        end
    
        try
            if isempty(row)
                % There is no slave with the current product code and revision
                % Update these fields
                idx = find(strcmp(models(:,1), selected_model),1);
                set_param(obj.block, 'product_code',    int2str(models{idx,2}));
                set_param(obj.block, 'revision_number', int2str(models{idx,3}));
                display([obj.block ': updated product code and revision number.'])
            else
                set_param(obj.block, 'model', models{row,1});
                display([obj.block ': updated model.'])
            end
        catch
            display([obj.block ' is corrupted. Replace it.'])
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateCustomDCEnable(obj)
        % This function sets the enable state of all mask variables
        % where custom values for distributed clock can be entered.
        % if dc_mode == 'Custom' they are enabled
        %
        % Requires the following mask varialbes:
        %       dc_mode, dc_*

        en    = get_param(obj.block,'MaskEnables');
        en_prev = en;
    
        if strcmp(get_param(obj.block,'dc_mode'),'Custom')
            state = 'on';
        else
            state = 'off';
        end
    
        mode_idx = find(strcmp(obj.maskNames,'dc_mode'));
        n = setdiff(find(strncmp(obj.maskNames,'dc_',3)), mode_idx);
        en(n) = repmat({state}, size(n));
        if ~isequal(en_prev,en)
            set_param(obj.block,'MaskEnables',en);
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateDCVisibility(obj,state)
        %% Change the visibility of all mask variables
        % starting with 'dc_' depending on <state>
        %
        % Requires the following mask varialbes:
        %       dc_mode, dc_*

        vis   = get_param(obj.block,'MaskVisibilities');
        vis_prev = vis;
    
        if state
            state = 'on';
        else
            state = 'off';
        end
    
        n = find(strncmp(obj.maskNames,'dc_',3));
        vis(n) = repmat({state}, size(n));
        if ~isequal(vis_prev,vis)
            set_param(obj.block,'MaskVisibilities',vis);
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    function updateSDOVisibility(obj,sdo)
        %% Change visibility of SDO Mask Variables
        % SDO Variables start with 'x8'
        % Argument:
        %       sdo: array of vars [Index, SubIndex]
        %
        % Requires the following mask varialbes:
        %       x8*

        vis   = get_param(obj.block,'MaskVisibilities');
        vis_prev = vis;
    
        n_all = find(strncmp(obj.maskNames,'x8',2));
        vis(n_all) = repmat({'off'}, size(n_all));
    
        sdo = strcat('x', dec2base(sdo(:,1), 16, 4),...
                     '_', dec2base(sdo(:,2), 16, 2));
        n = cellfun(@(x) find(strcmp(obj.maskNames,x)), cellstr(lower(sdo)));
        vis(n) = repmat({'on'},size(n));
    
        if ~isequal(vis_prev,vis)
            set_param(obj.block,'MaskVisibilities',vis);
        end
    end
end

end     % classdef

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = strJoin(s,delim)
    rv = cell2mat(strcat(reshape(s,1,[]),delim));
    rv(end) = [];
end