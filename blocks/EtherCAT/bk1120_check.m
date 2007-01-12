function bk1120_check(section)
    en = cell2struct(...
            get_param(gcb,'MaskEnables'),...
            get_param(gcb,'MaskNames')...
            );
    values = cell2struct(...
            get_param(gcb,'MaskValues'),...
            get_param(gcb,'MaskNames')...
            );

    switch section
    case 'op_dtype'
        if max(strcmp(values.op_dtype,{ 'double' 'single' }))
            en.scaling = 'on';
            en.filter = 'on';
        else
            set_param(gcb, 'scaling', 'off');
            set_param(gcb, 'filter', 'off');
            en.scaling = 'off';
            en.filter = 'off';
            en.scale = 'off';
            en.offset = 'off';
            en.tau = 'off';
        end
    case 'scaling'
        % Gain and Offset are only allowed with this Data Type
        if strcmp(values.scaling, 'on');
            en.scale = 'on';
            en.offset = 'on';
        else
            en.scale = 'off';
            en.offset = 'off';
        end
    case 'filter'
        if (strcmp(values.filter,'on'))
            en.tau = 'on';
        else
            en.tau = 'off';
        end
    end

    set_param(gcb,'MaskEnables',struct2cell(en));
