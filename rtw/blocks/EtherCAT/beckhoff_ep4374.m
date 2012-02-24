function output = beckhoff_ep4374(command,varargin)
    models = struct(...
      'EP43740002r00100002'          ,{{hex2dec('11164052'), hex2dec('00100002'), 'differential', 'EtherCATInfo_ep4xxx', 0, 4 }} ...
    );
    switch command
        case 'update'
            set_modelname(models,varargin{1});
            output = [];
        case 'getmodel'
            if isfield(models,varargin{1})
                output = models.(varargin{1}); 
            else
                output = [];
                errordlg([gcb ': Slave model "' varargin{1} '" is not implemented'])
            end
        case 'loaddescription' 
            selection = varargin{1};
            EtherCATInfoFile = selection{4};
            try 
                Infodata = load(EtherCATInfoFile);
            catch
                errordlg([gcb ': Could not load EtherCATInfo MAT file '...
                        selection{4}]);
            end
            Infonames = fieldnames(Infodata);
            description = getfield(Infodata,Infonames{1});
            device = description(selection{1},selection{2});
            if isempty(device)
                errordlg(sprintf(...
                ['%s: Could not find device with\n'...
                'ProductCode \t#x%08X\n'...
                'RevisionNo \t#x%08X\n'...
                'in file %s.'],...
                gcs, selection{1}, selection{2}, EtherCATInfoFile));
            end
            output = device;
        case 'process'
            output = prepare_config(varargin{:});
        case 'dtype'
            en = cell2struct(...
                   get_param(gcb,'MaskEnables'),...
                   get_param(gcb,'MaskNames')...
                 );
            values = cell2struct(...
                get_param(gcb,'MaskValues'),...
                get_param(gcb,'MaskNames')...
                );            
            % Gain and Offset are only allowed with this Data Type
            if strcmp(values.dtype, 'Double with scale and offset');
                en.input_scale = 'on';
                en.input_offset = 'on';
            else
                en.input_scale = 'off';
                en.input_offset = 'off';
                if ~strcmp(get_param(gcb, 'input_scale'), '1')
                    set_param(gcb, 'input_scale', '1');
                end
                if ~strcmp(get_param(gcb, 'input_offset'), '0')
                    set_param(gcb, 'input_offset', '0');
                end
            end
            if strcmp(values.dtype, 'Raw bits')
                set_param(gcb,'filter','off');
                en.filter = 'off';
                en.tau = 'off';
            else
                en.filter = 'on';
            end
            set_param(gcb,'MaskEnables',struct2cell(en));
            set_param(gcb,'MaskValues',struct2cell(values));  
            output = [];
        case 'filter'
            en = cell2struct(...
                   get_param(gcb,'MaskEnables'),...
                   get_param(gcb,'MaskNames')...
                 );
            values = cell2struct(...
                get_param(gcb,'MaskValues'),...
                get_param(gcb,'MaskNames')...
                );                        
            if (strcmp(values.filter,'on'))
                en.tau = 'on';
            else
                en.tau = 'off';
            end
            set_param(gcb,'MaskEnables',struct2cell(en));
            set_param(gcb,'MaskValues',struct2cell(values));   
            output = [];
        case 'dc'
            en = cell2struct(...
                get_param(gcb,'MaskEnables'),...
                get_param(gcb,'MaskNames')...
                );
            values = cell2struct(...
                get_param(gcb,'MaskValues'),...
                get_param(gcb,'MaskNames')...
                );
            if strcmp(values.dcmode, 'DC-Customized');
                en.dccustomer = 'on';
            else
                en.dccustomer = 'off';
            end;
            set_param(gcb,'MaskEnables',struct2cell(en));
            set_param(gcb,'MaskValues',struct2cell(values));                                              
            output=[];
        otherwise
            disp('Unknown method.')
            output = [];
    end
end

%%
function set_modelname(models,block)
     stylestr_model= textscan(get_param(block,'MaskStyleString'),'%s','Delimiter',',');
     names = fieldnames(models);
     namearray = strjoin(names,'|');
     modelstr = strcat('popup(',namearray,')');
     stylestr_model{1}(3) = cellstr(modelstr);
     stylestr_new = strjoin(stylestr_model{1},',');
     set_param(block,'MaskStyleString',stylestr_new);
end

%%
function config = prepare_config(varargin)

    selection = varargin{1};
    device = varargin{2};
    type_1 = varargin{3};
    type_2 = varargin{4};
    type_3 = varargin{5};
    type_4 = varargin{6};
    dcmode = varargin{7};
    dccustom = varargin{8};
    io_type = varargin{9};
    output_scale = varargin{10};
    input_scale = varargin{11};
    input_offset = varargin{12};
    input_dtype = varargin{13};
    filter = varargin{14};
    tau = varargin{15};
    tsample = varargin{16};
    
    config.SdoConfig = [];
    config.SoeConfig = [];
    config.IO = [];
    config.SlaveConfig = genSlaveConfig(device);
    SdoConfig = [];
    channels = selection{6};    
    
    %build type array
    basetype=[];
    basetype = [type_1-1; type_2-1; type_3-1; type_4-1];
    for i=1:channels
        if basetype(i)==3
            basetype(i) = 6; %0-10V
        end
    end
    
    
    for i=0:(channels-1)
        SdoConfig = [...
            SdoConfig;...
            hex2dec('F800'),i+1, 16, basetype(i+1);...% Set Input Type
            ]; 
    end 

    for i=[3 4] 
        SdoConfig = [...
            SdoConfig;...
		     hex2dec('8000')+hex2dec('10')*(i-1), hex2dec('2'),  8, 0;...% signed representation
		     hex2dec('8000')+hex2dec('10')*(i-1), hex2dec('13'),  16, 0;...% default value
            ]; 
    end 
    
    PdoEntries = [hex2dec('6000') hex2dec('11');...
		  hex2dec('6010') hex2dec('11');...
		  hex2dec('7020') hex2dec('11');...
		  hex2dec('7030') hex2dec('11');...
                 ];
   
    
    
    if io_type == 1 %Vector
        %First Input 
        config.IO.Port(1).PdoEntry = PdoEntries(1:2,:);
        if input_dtype ~= 1
            config.IO.Port(1).PdoFullScale = 2^15;
            if input_dtype == 3
                if ~isempty(input_scale)
                    config.IO.Port(1).Gain = input_scale(min(1,max(size(input_scale))));
                    config.IO.Port(1).GainName = 'InputFullScale';
                end
                if ~isempty(input_offset)
                    config.IO.Port(1).Offset = input_offset(min(1,max(size(input_offset))));
                    config.IO.Port(1).OffsetName = 'InputOffset';
                end
            end
            if filter
                if sum(tau == 0)
                errordlg([gcb ': Specify a nonzero time constant '...
                    'for the output filter.']);
                end
                if (tsample)
                    config.IO.Port(1).Filter = tsample./tau;
                    config.IO.Port(1).FilterName = 'InputWeight';
                else
                    config.IO.Port(1).Filter = 2*pi./tau;
                    config.IO.Port(1).FilterName = 'Omega';
                end
            end
        end
        %Now Output 
        config.IO.Port(2).PdoEntry = PdoEntries(3:4,:);
        config.IO.Port(2).PdoFullScale = 2^15;
        if ~isempty(output_scale)
            config.IO.Port(2).Gain = output_scale(min(1,max(size(output_scale))));
            config.IO.Port(2).GainName = 'OutputFullScale';
        end
	
    elseif io_type == 2 %Single

	  for i=1:2
        config.IO.Port(i).PdoEntry = PdoEntries(i,:);
        if input_dtype ~= 1
            config.IO.Port(i).PdoFullScale = 2^15;
            if input_dtype == 3
                if ~isempty(input_scale)
                    config.IO.Port(i).Gain = input_scale(min(i,max(size(input_scale))));
                    config.IO.Port(i).GainName = ['InputFullScale' num2str(i)];
                end
                if ~isempty(input_offset) 
                   config.IO.Port(i).Offset = input_offset(min(i,max(size(input_offset))));
                   config.IO.Port(i).OffsetName = ['InputOffset' num2str(i)];
                end
            end
            if filter
                if sum(tau == 0)
                    errordlg([gcb ': Specify a nonzero time constant '...
                    'for the output filter.']);
                end
                if (tsample)
                    config.IO.Port(1).Filter = tsample./tau;
                    config.IO.Port(1).FilterName = 'InputWeight';
                else
                    config.IO.Port(1).Filter = 2*pi./tau;
                    config.IO.Port(1).FilterName = 'Omega';
                end
            end	  
         end
      end
      for i=3:4
        config.IO.Port(i).PdoEntry = PdoEntries(i,:);
        config.IO.Port(i).PdoFullScale = 2^15;
        if ~isempty(output_scale)
            config.IO.Port(i).Gain = output_scale(min(i,max(size(output_scale))));
            config.IO.Port(i).GainName = ['OutputFullScale' num2str(i)];
        end
      end
      
    end
    
    if dcmode == 3 % Customized parameter
        config.IO.DcOpMode = dccustom;  
    else
        config.IO.DcOpMode = dcmode-1;
    end
    config.SdoConfig = cell2struct(...
    num2cell(SdoConfig),...
    {'Index' 'SubIndex' 'BitLen' 'Value'}, 2);

end
