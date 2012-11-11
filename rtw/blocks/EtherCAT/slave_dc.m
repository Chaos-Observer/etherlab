function rv = slave_dc(method,dc,varargin)
%% This function updates the block mask according to various methods:
%% 'supported': varargin = {bool}
%%       Slave supports DC or not
%% 

rv = [];

dc_vars = {'dc_mode',    'dc_assign_activate', ...
           'dc_cycle0',  'dc_cycle0_factor', ...
           'dc_shift0',  'dc_shift0_factor', 'dc_shift0_input', ...
           'dc_cycle1',  'dc_cycle1_factor', ...
           'dc_shift1',  'dc_shift1_factor'};

mode = get_param(gcbh,'dc_mode');

switch method
case 'ui'
    enable = ~isempty(dc);

    if ~enable
        update_mask_state('MaskEnables', dc_vars, false);
        return
    end

    update_mask_state('MaskEnables', dc_vars(1), true);


    if strcmp(mode, 'Custom');
        update_mask_state('MaskEnables', dc_vars, true);
    else
        if isempty(find(strcmp(dc(:,1), mode),1))
            set_param(gcbh, 'dc_mode', dc{1,1});
        end
        update_mask_state('MaskEnables', dc_vars(2:end), false);
    end

case 'config'
    if isempty(dc)
        rv = [];
    else
        idx = find(strcmp(dc(:,1),mode));
        if strcmp(mode, 'Custom')
            rv = varargin{1};
        elseif ~isempty(idx)
            rv = dc{idx,2};
        end
    end
end
