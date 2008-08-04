function el5101_check(section)
    en = cell2struct(...
            get_param(gcb,'MaskEnables'),...
            get_param(gcb,'MaskNames')...
            );
    values = cell2struct(...
            get_param(gcb,'MaskValues'),...
            get_param(gcb,'MaskNames')...
            );

    switch section
    case 'IndexReset'
        if strcmp(values.reset, 'on')
            en.reload      = 'off';
            set_param(gcb, 'reload', 'off');
        else
            en.reload      = 'on';
        end
    case 'Reload'
        if strcmp(values.reload, 'on')
            en.reset      = 'off';
            set_param(gcb, 'reset', 'off');
            en.reloadvalue = 'on';
        else
            en.reset      = 'on';
            en.reloadvalue = 'off';
        end
    case 'Freq'
        if strcmp(values.freq, 'on')
            en.freqwin = 'on';
        else
            en.freqwin = 'off';
        end
    case 'Latch'
        if strcmp(values.latch, 'on')
            en.gate = 'on';
        else
            en.gate = 'off';
        end
    end

    set_param(gcb,'MaskEnables',struct2cell(en));
