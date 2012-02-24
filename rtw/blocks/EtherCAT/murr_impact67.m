function output = murr_impact67(command,varargin)
   models = struct(...
        'DI8DO8r00000001'          ,{{hex2dec('0000D72A'), hex2dec('00000001'), 'DI8DO8 24V (9A)' , 'EtherCATInfo_murrimpact67', 8 , 8}}, ...
        'DO16r00000001'            ,{{hex2dec('0000D72C'), hex2dec('00000001'), 'DO16 24 (9A)V'   , 'EtherCATInfo_murrimpact67', 0 ,16}}, ...
        'DO8r00000001'             ,{{hex2dec('0000D72B'), hex2dec('00000001'), 'DO8 24V (9A)'    , 'EtherCATInfo_murrimpact67', 0 , 8}}, ...
        'DI16r00000001'            ,{{hex2dec('0000D729'), hex2dec('00000001'), 'DI16 24V'        , 'EtherCATInfo_murrimpact67', 16 , 0}} ... 
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
            %                       selection  , device    , status   ,
            %                       dtype
            output = prepare_config(varargin{1},varargin{2},varargin{3},varargin{4});
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
function config = prepare_config(selection,device,status,dtype)
    
    config.SdoConfig = [];
    config.SoeConfig = [];
    config.IO = [];
    config.SlaveConfig = genSlaveConfig(device);
    
    channels_input = selection{6};
    channels_output = selection{5};
    PdoEntries_input = [];
    PodEntries_output = [];
    StatusPdoEntries_input = [];
    StatusPdoEntries_output = [];    
    
    %output.Pdo.Exclude = [hex2dec('1A01') + 2*(0:(channels-1))];

    if channels_input >0
        PdoEntries_input = [hex2dec('6200')+ zeros(1,channels_input); 1:channels_input]';
        if status
            StatusPdoEntries_input = [hex2dec('1001') ; 3]';
        end
    end
    
    if channels_output >0
        PdoEntries_output = [hex2dec('6000') + zeros(1,channels_output); 1:channels_output]';
        if status
            StatusPdoEntries_output = [hex2dec('1001') ; 4]';
        end
    end

    config.IO.Port = [];
    channelcounter = 1;
    
    if dtype == 2 %seperate 
        if channels_input >0
            config.IO.Port = [ config.IO.Port;... 
                          cell2struct(mat2cell(PdoEntries_input,ones(1,channels_input),2),'PdoEntry',2);...
                        ];
        end
        if channels_output >0
        config.IO.Port = [ config.IO.Port;...
                          cell2struct(mat2cell(PdoEntries_output,ones(1,channels_output),2),'PdoEntry',2);...
                        ];
        end
    
        if status
            if channels_input >0
                config.IO.Port = [ config.IO.Port;...
                             cell2struct(mat2cell(StatusPdoEntries_input,ones(1,1),2),'PdoEntry',2);...
                           ];
            end
            if channels_output >0
                config.IO.Port = [ config.IO.Port;...
                             cell2struct(mat2cell(StatusPdoEntries_output,ones(1,1),2),'PdoEntry',2);...
                           ];
 
            end
        end
    end
    
    if dtype == 1 %Vector
        
         if channels_input >0
             config.IO.Port(channelcounter).PdoEntry = PdoEntries_input;
             channelcounter = channelcounter +1;             
         end
        
         if channels_output >0
             config.IO.Port(channelcounter).PdoEntry = PdoEntries_output;
             channelcounter = channelcounter +1;             
         end
        
        
         if status
             if channels_input >0
                config.IO.Port(channelcounter).PdoEntry = StatusPdoEntries_input;
                channelcounter = channelcounter +1;  
             end
             
             if channels_output >0
                config.IO.Port(channelcounter).PdoEntry = StatusPdoEntries_output;
                channelcounter = channelcounter +1;  
             end 
         end
   
    end
    
end
