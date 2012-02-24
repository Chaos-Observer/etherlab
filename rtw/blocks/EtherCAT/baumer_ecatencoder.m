function output = baumer_ecatencoder(command,varargin)
    models = struct(...
        'BTATD4r00000001'          ,{{hex2dec('00000001'), hex2dec('00000001'), 'BT ATD4 EtherCAT Encoder', 'EtherCATInfo_baumerecatencoder', 0, 1 }}, ...
        'BTATD4r00000002'          ,{{hex2dec('00000001'), hex2dec('00000002'), 'BT ATD4 EtherCAT Encoder', 'EtherCATInfo_baumerecatencoder', 0, 1 }}, ...
        'BTATD2r00000001'          ,{{hex2dec('00000002'), hex2dec('00000001'), 'BT ATD2 EtherCAT Encoder', 'EtherCATInfo_baumerecatencoder', 0, 1 }}, ...
        'BTATD2r00000002'          ,{{hex2dec('00000002'), hex2dec('00000002'), 'BT ATD2 EtherCAT Encoder', 'EtherCATInfo_baumerecatencoder', 0, 1 }}, ...
        'BTATD2POEr00000001'       ,{{hex2dec('00000003'), hex2dec('00000001'), 'BT ATD2 PoE EtherCAT Encoder', 'EtherCATInfo_baumerecatencoder', 0, 1 }}, ...
        'BTATD2POEr00000002'       ,{{hex2dec('00000003'), hex2dec('00000002'), 'BT ATD2 PoE EtherCAT Encoder', 'EtherCATInfo_baumerecatencoder', 0, 1 }} ...
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
            %                       selection  , device    , dcmode     ,
            %                       direction
            output = prepare_config(varargin{1},varargin{2},varargin{3},...
                                    varargin{4},varargin{5});
        case 'model'
            en = cell2struct(...
                get_param(gcb,'MaskEnables'),...
                get_param(gcb,'MaskNames')...
                );
            values = cell2struct(...
                get_param(gcb,'MaskValues'),...
                get_param(gcb,'MaskNames')...
                );
            if str2double(values.model(end)) == 1
                en.dcmode='off';
            else
                en.dcmode='on';
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
                en.dccustom = 'on';
            else
                en.dccustom = 'off';
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
function config = prepare_config(selection,device,dcmode,dccustom, direction)
    config.SdoConfig = [];
    config.SoeConfig = [];
    config.IO = [];
    config.SlaveConfig = genSlaveConfig(device);
    SdoConfig = [];
    
    
   
    SdoConfig = [...
            SdoConfig;...
            hex2dec('6000'), hex2dec('0'),  16, (direction-1)+4;...% Disable Hardware Filter
            %Note the Scaling (Bit 2) function has to be to be enabled,
            %Firmwarebug!!!!
            ]; 
   
    
    
    PdoEntries = [hex2dec('6004'); hex2dec('0')]';
    
    %config.IO.Pdo.Exclude = [hex2dec('1A01') + 2*(0:(channels-1))];

    config.IO.SlaveConfig = genSlaveConfig(device);
   
    config.IO.Port(1).PdoEntry = PdoEntries;
    
    if selection{2} == 2
        if dcmode == 3 %DC-Custom
             %Set Index of DC Mode or set [Index AssignActivate cycletimesync0 factor0 cycletimeshift0 input0 cycletimesync1 factor1 cylcetimeshit1]
             config.IO.DcOpMode = dccustom;
        else
            config.IO.DcOpMode = dcmode-1; 
        end
    end
    config.SdoConfig = cell2struct(...
    num2cell(SdoConfig),...
    {'Index' 'SubIndex' 'BitLen' 'Value'}, 2);

end