function output = beckhoff_el320x(command,varargin)
    models = struct(...
        'EL3201r00100000'          ,{{hex2dec('c813052'), hex2dec('00100000'), 'RTD Input', 'EtherCATInfo_el3xxx', 1, 0 }}, ...
        'EL3201r00110000'          ,{{hex2dec('c813052'), hex2dec('00110000'), 'RTD Input', 'EtherCATInfo_el3xxx', 1, 0 }}, ...
        'EL3202r00100000'          ,{{hex2dec('c823052'), hex2dec('00100000'), 'RTD Input', 'EtherCATInfo_el3xxx', 2, 0 }}, ...
        'EL3202r00110000'          ,{{hex2dec('c823052'), hex2dec('00110000'), 'RTD Input', 'EtherCATInfo_el3xxx', 2, 0 }}, ...
        'EL3204r00100000'          ,{{hex2dec('c843052'), hex2dec('00100000'), 'RTD Input', 'EtherCATInfo_el3xxx', 4, 0 }}, ...
        'EL3204r00110000'          ,{{hex2dec('c843052'), hex2dec('00110000'), 'RTD Input', 'EtherCATInfo_el3xxx', 4, 0 }} ...
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
            %                       selection  , device    , 
            %                       rtd_element_1, connection_type_1,
            %                       rtd_element_2, connection_type_2,
            %                       rtd_element_3, connection_type_3,
            %                       rtd_element_4, connection_type_4,
            %                       enable_filter, enable_status
            output = prepare_config(varargin{1},varargin{2},varargin{3},...
                                    varargin{4},varargin{5},varargin{6},...
                                    varargin{7},varargin{8},varargin{9},...
                                    varargin{10},varargin{11},varargin{12});
        case 'model'
            en = cell2struct(...
                get_param(gcb,'MaskEnables'),...
                get_param(gcb,'MaskNames')...
                );
            values = cell2struct(...
                get_param(gcb,'MaskValues'),...
                get_param(gcb,'MaskNames')...
                );
            % Select 1,2 or 4 channel terminal
            if str2double(values.model(6)) == 1
                en.rtd_element_2='off';
                en.connection_type_2='off';
                en.rtd_element_3='off';
                en.connection_type_3='off';
                en.rtd_element_4='off';
                en.connection_type_4='off';
            end
            if str2double(values.model(6)) == 2
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
            if str2double(values.model(6)) == 4
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
            
            set_param(gcb,'MaskEnables',struct2cell(en));
            set_param(gcb,'MaskValues',struct2cell(values));
            output = [];
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
function config = prepare_config(selection,device,rtd_element_1, connection_type_1, rtd_element_2, connection_type_2, rtd_element_3, connection_type_3, rtd_element_4, connection_type_4,enable_filter, enable_status)
    
    config.SdoConfig = [];
    config.SoeConfig = [];
    config.IO = [];
    config.SlaveConfig = genSlaveConfig(device);
    SdoConfig = [];
    
    channels = selection{5};    

   if enable_filter == 1
      SdoConfig = [...
           SdoConfig;...
           hex2dec('8000'), hex2dec('6'),  8, enable_filter;... % Enable Filter for all Channels
           hex2dec('8000'), hex2dec('15'),  16, 0;...           % Default Filter Freq=50Hz
         ];
    end;    
    
    %Get Connection Type Index
    connect=[];
    connect(1)=connection_type_1-1;     
    switch connection_type_2
        case 1
            connect(2)=0;
        case 2
            connect(2)=1;
        case 3
            connect(2)=3;
    end;
    switch connection_type_3
        case 1
            connect(3)=0;
        case 2
            connect(3)=3;
    end;
    switch connection_type_4
        case 1
            connect(4)=0;
        case 2
            connect(4)=3;
    end;    
    
    rtd_element = [];
    rtd_element(1)=rtd_element_1;
    rtd_element(2)=rtd_element_2;
    rtd_element(3)=rtd_element_3;
    rtd_element(4)=rtd_element_4;
    
    for i=1:channels
        %Configuration of the RTD-Element and the connection type and general settings
        SdoConfig = [...
            SdoConfig;...
            hex2dec('8000')+ hex2dec('10')*(i-1), hex2dec('19'),  16, (rtd_element(i)-1);... % RTD-Element Channel 
            hex2dec('8000')+ hex2dec('10')*(i-1), hex2dec('1A'),  16, connect(i);... % Connection Type Channel 
            hex2dec('8000')+ hex2dec('10')*(i-1), hex2dec('1'),  8, 0;... % Disable user scaling Channel 
            hex2dec('8000')+ hex2dec('10')*(i-1), hex2dec('2'),  8, 0;... % Value as signed int 16 Channel 
            hex2dec('8000')+ hex2dec('10')*(i-1), hex2dec('7'),  8, 0;... % Disable Limit 1 Channel 
            hex2dec('8000')+ hex2dec('10')*(i-1), hex2dec('8'),  8, 0;... % Disable Limit 2 Channel 
            hex2dec('8000')+ hex2dec('10')*(i-1), hex2dec('A'),  8, 0;... % Disable user callibration Channel 
            hex2dec('8000')+ hex2dec('10')*(i-1), hex2dec('B'),  8, 1;... % Enable vendor callibration Channel 
        ];    
    end
    
    
        
    if enable_status
        StatusPdoEntries = [hex2dec('6000') + hex2dec('10')*(0:(channels-1)); hex2dec('7')*ones(1,channels)]';
        PdoEntries = [hex2dec('6000') + hex2dec('10')*(0:(channels-1)); hex2dec('11')*ones(1,channels)]';
    else
        PdoEntries = [hex2dec('6000') + hex2dec('10')*(0:(channels-1)); hex2dec('11')*ones(1,channels)]';
    end
    
    %config.IO.Pdo.Exclude = [hex2dec('1A01') + 2*(0:(channels-1))];

    config.IO.SlaveConfig = genSlaveConfig(device);
    

   config.IO.Port(1).PdoEntry = PdoEntries;

   if enable_status
       config.IO.Port(2).PdoEntry = StatusPdoEntries;
   end

   config.IO.Port(1).PdoFullScale = [2^15*ones(1,channels)]';
   config.IO.Port(1).Gain = [3276.70*ones(1,channels)]';
   config.IO.Port(1).Offset = [0*ones(1,channels)]';
   config.IO.Port(1).GainName = 'FullScale';
   config.IO.Port(1).OffsetName = 'Offset';
  
    config.SdoConfig = cell2struct(...
    num2cell(SdoConfig),...
    {'Index' 'SubIndex' 'BitLen' 'Value'}, 2);

end
