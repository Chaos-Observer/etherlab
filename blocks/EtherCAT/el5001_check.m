function el5001_check(section)
    en = cell2struct(...
            get_param(gcb,'MaskEnables'),...
            get_param(gcb,'MaskNames')...
            );
    values = cell2struct(...
            get_param(gcb,'MaskValues'),...
            get_param(gcb,'MaskNames')...
            );

    switch section
    case 'dtype'
        % Gain and Offset are only allowed with this Data Type
        if strcmp(values.dtype, 'Double with scale and offset');
            en.scale = 'on';
            en.offset = 'on';
        else
            en.scale = 'off';
            en.offset = 'off';
            if ~strcmp(get_param(gcb, 'scale'), '1')
                set_param(gcb, 'scale', '1');
            end
            if ~strcmp(get_param(gcb, 'offset'), '0')
                set_param(gcb, 'offset', '0');
            end
        end
        if strcmp(values.dtype, 'Raw bits')
            set_param(gcb,'filter','off');
            en.filter = 'off';
            en.tau = 'off';
        else
            en.filter = 'on';
        end
    case 'filter'
        if (strcmp(values.filter,'on'))
            en.tau = 'on';
        else
            en.tau = 'off';
        end
    case 'inhibit'
        if (strcmp(values.inhibit,'on'))
            en.inh_time = 'on';
        else
            en.inh_time = 'off';
        end
    case 'frame'
        if (strncmp(values.frame,'Variable',8))
            en.size = 'on';
        else
            en.size = 'off';
        end
    case 'inh_time'
        if (str2num(values.inh_time) > 65535)
            set_param(gcb,'inh_time','65535');
            error('Inhibit time cannot exceed 65536us');
        elseif (str2num(values.inh_time) < 1)
            set_param(gcb,'inh_time','1');
            error('Minimum inhibit time is 1us');
        end
    end

    set_param(gcb,'MaskEnables',struct2cell(en));
