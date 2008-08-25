function el320x_check(section)
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
        % Select 1,2 or 4 channel terminal
	  if strcmp(values.model, 'EL3201');
            en.rtd_element_2='off';
            en.connection_type_2='off';
            en.rtd_element_3='off';
            en.connection_type_3='off';
            en.rtd_element_4='off';
            en.connection_type_4='off';
          end
	  if strcmp(values.model, 'EL3202');
            en.rtd_element_2='on';
            en.connection_type_2='on';
            en.rtd_element_3='off';
            en.connection_type_3='off';
            en.rtd_element_4='off';
            en.connection_type_4='off';
            if strcmp(values.connection_type_1, '4-wire');
               values.connection_type_1='3-wire';
            end
          end
	  if strcmp(values.model, 'EL3204');
            en.rtd_element_2='on';
            en.connection_type_2='on';
            en.rtd_element_3='on';
            en.connection_type_3='on';
            en.rtd_element_4='on';
            en.connection_type_4='on';
            if ~strcmp(values.connection_type_1, 'not connected');
                values.connection_type_1='2-wire';
            end;
            if ~strcmp(values.connection_type_2, 'not connected');
                values.connection_type_2='2-wire';
            end;

          end
    end

    set_param(gcb,'MaskEnables',struct2cell(en));
    set_param(gcb,'MaskValues',struct2cell(values));
