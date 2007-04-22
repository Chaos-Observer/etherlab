function el2xxx_check(section)
    en = cell2struct(...
            get_param(gcb,'MaskEnables'),...
            get_param(gcb,'MaskNames')...
            );
    values = cell2struct(...
            get_param(gcb,'MaskValues'),...
            get_param(gcb,'MaskNames')...
            );

    switch section
    case 'model'
        % Presumably all EL203x will provide a status output
        if strncmp(values.model, 'EL203',5) && ...
                ~strcmp(values.input_type,'Separate Inputs')
            en.status = 'on';
        else
            en.status = 'off';
            set_param(gcb, 'status', 'off');
        end
    case 'input_type'
        if strncmp(values.model, 'EL203',5) && ...
                ~strcmp(values.input_type,'Separate Inputs')
            en.status = 'on';
        else
            en.status = 'off';
            set_param(gcb, 'status', 'off');
        end
    end

    set_param(gcb,'MaskEnables',struct2cell(en));
