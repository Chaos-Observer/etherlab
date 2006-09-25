function el41xx_check(section)
    en = cell2struct(...
            get_param(gcb,'MaskEnables'),...
            get_param(gcb,'MaskNames')...
            );
    values = cell2struct(...
            get_param(gcb,'MaskValues'),...
            get_param(gcb,'MaskNames')...
            );

    switch section
    case 'input_type'
        % When not in vector input, no status outputs
        if strcmp(values.input_type, 'Vector Input')
            en.status = 'off';
        else
            en.status = 'off';
            set_param(gcb, 'status', 'off');
        end
    end

    set_param(gcb,'MaskEnables',struct2cell(en));
