function output = beckhoff_el515x(command,varargin)
  %disp(['Call function: ' command])
    models = struct(...
        'EL5151r00100000'          ,{{hex2dec('141f3052'), hex2dec('00100000'), 'Inc Encoder', 'EtherCATInfo_el5xxx', 1, 0 }}, ...
        'EL5151r00110000'          ,{{hex2dec('141f3052'), hex2dec('00110000'), 'Inc Encoder', 'EtherCATInfo_el5xxx', 1, 0 }}, ...
        'EL5151r00130000'          ,{{hex2dec('141f3052'), hex2dec('00130000'), 'Inc Encoder', 'EtherCATInfo_el5xxx', 1, 0 }}, ...
        'EL5152r00100000'          ,{{hex2dec('14203052'), hex2dec('00100000'), 'Inc Encoder', 'EtherCATInfo_el5xxx', 2, 0 }} ...
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
            output = prepare_config(varargin{1},varargin{2},varargin{3},...
                                    varargin{4},varargin{5},varargin{6},...
                                    varargin{7},varargin{8},varargin{9},...
                                    varargin{10},varargin{11},varargin{12},...
                                    varargin{13},varargin{14},varargin{15},...
                                    varargin{16},varargin{17},varargin{18},...
                                    varargin{19});
        case 'model'
            en = cell2struct(...
                get_param(gcb,'MaskEnables'),...
                get_param(gcb,'MaskNames')...
                );
            values = cell2struct(...
                get_param(gcb,'MaskValues'),...
                get_param(gcb,'MaskNames')...
                );
            if isfield(models,values.model)
                myselection = models.(values.model); 
                if myselection{2} == hex2dec('00100000')
                    en.dcmode = 'off';
                else
                    en.dcmode = 'on';
                end
            end
            if (strcmp(values.model, 'EL5151r00100000') && strcmp(values.usage,'Frequency'))
                errordlg([gcb ': Rev '...
                         values.model...
                        ' does not support frequency! mode']);
                values.usage = 'Increment';
            end
            if str2double(values.model(6)) == 1
                en.enablecreset='on';
                en.enableextreset='on';
                en.gatepolarity = 'on';
                en.extresetpolarity = 'on';
            else
                en.enablecreset='off';
                en.enableextreset='off';
                en.gatepolarity = 'off';
                en.extresetpolarity = 'off';
            end
            set_param(gcb,'MaskEnables',struct2cell(en));
            set_param(gcb,'MaskValues',struct2cell(values));       
            output=[];
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
        case 'usage'
            en = cell2struct(...
                get_param(gcb,'MaskEnables'),...
                get_param(gcb,'MaskNames')...
                );
            values = cell2struct(...
                get_param(gcb,'MaskValues'),...
                get_param(gcb,'MaskNames')...
                );
            if (strcmp(values.model, 'EL5151r00100000') && strcmp(values.usage,'Frequency'))
                errordlg([gcb ': Rev '...
                         values.model...
                        ' does not support frequency! mode']);
                values.usage = 'Increment';
            end
            set_param(gcb,'MaskEnables',struct2cell(en));
            set_param(gcb,'MaskValues',struct2cell(values));       
            output=[];
        otherwise
            disp('Unknown method.')
            output=[];
    end
    %disp('end of function')
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
function config = prepare_config(selection,device,dcmode,dccustom,usage,...
        enableextreset,...
        extresetpolarity,enablecreset,enableupdown,gatepolarity,...
        enablemicroinc,revertrotation,freqwindowbase,freqwindow,...
        freqscaling,freqresolution,freqwaittime,periodscaling,...
        periodresolution)
    
    config.SdoConfig = [];
    config.SoeConfig = [];
    config.IO = [];
    config.SlaveConfig = genSlaveConfig(device);
    SdoConfig = [];
    
    channels = selection{5};    
    
    if channels == 1 % only for El5151
        SdoConfig = [...
            SdoConfig;...
            hex2dec('8000'), hex2dec('1'),  8, enablecreset;...
            hex2dec('8000'), hex2dec('2'),  8, enableextreset;...
            hex2dec('8000'), hex2dec('4'),  8, gatepolarity-1;...
            hex2dec('8000'), hex2dec('10'),  8, extresetpolarity-1;...
            ];              
    end
    
    
    for i=0:(channels-1)
    SdoConfig = [...
            SdoConfig;...
            hex2dec('8000')+hex2dec('10')*i, hex2dec('3'),  8, enableupdown;...
            hex2dec('8000')+hex2dec('10')*i, hex2dec('A'),  8, enablemicroinc;...
            hex2dec('8000')+hex2dec('10')*i, hex2dec('E'),  8, revertrotation;...
            hex2dec('8000')+hex2dec('10')*i, hex2dec('F'),  8, freqwindowbase-1;...
            hex2dec('8000')+hex2dec('10')*i, hex2dec('11'),  16, freqwindow;...
            hex2dec('8000')+hex2dec('10')*i, hex2dec('13'),  16, freqscaling;...
            hex2dec('8000')+hex2dec('10')*i, hex2dec('15'),  16, freqresolution;...
            hex2dec('8000')+hex2dec('10')*i, hex2dec('17'),  16, freqwaittime;...
            hex2dec('8000')+hex2dec('10')*i, hex2dec('13'),  16, periodscaling;...
            hex2dec('8000')+hex2dec('10')*i, hex2dec('16'),  16, periodresolution;...  
            ]; 
    end 
    
    PdoEntries = [];
    Excludes = [];
    
    switch usage
        case 1 %Increment
            %Outputs
            if channels == 1 %EL5151
                PdoEntries = [PdoEntries; [hex2dec('7000'); hex2dec('1')]']; % Enable Latch C
                PdoEntries = [PdoEntries; [hex2dec('7000'); hex2dec('2')]']; % Enable Latch extern on
                PdoEntries = [PdoEntries; [hex2dec('7000'); hex2dec('3')]']; % Set Counter
                PdoEntries = [PdoEntries; [hex2dec('7000'); hex2dec('4')]']; % Enable Latch extern on neg edge
                PdoEntries = [PdoEntries; [hex2dec('7000'); hex2dec('11')]']; % Set Counter Value
                PdoEntries = [PdoEntries; [hex2dec('6000'); hex2dec('1')]']; % Latch C valid
                PdoEntries = [PdoEntries; [hex2dec('6000'); hex2dec('2')]']; % Latch extern valid
                PdoEntries = [PdoEntries; [hex2dec('6000'); hex2dec('3')]']; % Set counter done
                if selection{2} ~= hex2dec('00100000')
                    PdoEntries = [PdoEntries; [hex2dec('6000'); hex2dec('8')]']; % Extrapolation stall
                end
                PdoEntries = [PdoEntries; [hex2dec('6000'); hex2dec('9')]']; % Status of Input A
                PdoEntries = [PdoEntries; [hex2dec('6000'); hex2dec('A')]']; % Status of input B 
                PdoEntries = [PdoEntries; [hex2dec('6000'); hex2dec('B')]']; % Status of Input C
                PdoEntries = [PdoEntries; [hex2dec('6000'); hex2dec('D')]']; % State extern latch
                PdoEntries = [PdoEntries; [hex2dec('6000'); hex2dec('11')]']; % Counter Value
                PdoEntries = [PdoEntries; [hex2dec('6000'); hex2dec('12')]']; % Latch Value
            else
                PdoEntries = [PdoEntries; [hex2dec('7000')+ hex2dec('10')*(0:(channels-1)); hex2dec('3')*ones(1,channels)]']; %Set Counter Set Counter
                PdoEntries = [PdoEntries; [hex2dec('7000')+ hex2dec('10')*(0:(channels-1)); hex2dec('11')*ones(1,channels)]']; % Set Counter Value
                PdoEntries = [PdoEntries; [hex2dec('6000')+ hex2dec('10')*(0:(channels-1)); hex2dec('3')*ones(1,channels)]']; % Set counter done
                PdoEntries = [PdoEntries; [hex2dec('6000')+ hex2dec('10')*(0:(channels-1)); hex2dec('8')*ones(1,channels)]']; % Extrapolation stall
                PdoEntries = [PdoEntries; [hex2dec('6000')+ hex2dec('10')*(0:(channels-1)); hex2dec('9')*ones(1,channels)]']; % Status of Input A
                PdoEntries = [PdoEntries; [hex2dec('6000')+ hex2dec('10')*(0:(channels-1)); hex2dec('A')*ones(1,channels)]']; % Status of input B 
                PdoEntries = [PdoEntries; [hex2dec('6000')+ hex2dec('10')*(0:(channels-1)); hex2dec('11')*ones(1,channels)]']; % Counter Value
            end
            Excludes=[Excludes;hex2dec('1A02')];
            if channels == 1 %EL5151
               if selection{2} ~= hex2dec('00100000')
	         Excludes=[Excludes;hex2dec('1A03')];
	      end
	    end
        case 2 %Frequency 
            PdoEntries = [PdoEntries; [hex2dec('7000'); hex2dec('3')]']; % Set Counter, Map for enable sync manager output Bug
            PdoEntries = [PdoEntries; [hex2dec('6000')+ hex2dec('10')*(0:(channels-1)); hex2dec('13')*ones(1,channels)]']; % Frequency Value
            Excludes=[Excludes;hex2dec('1A02')];
        case 3 %Period
            PdoEntries = [PdoEntries; [hex2dec('7000'); hex2dec('3')]']; % Set Counter, Map for enable sync manager output Bug
            PdoEntries = [PdoEntries; [hex2dec('6000')+ hex2dec('10')*(0:(channels-1)); hex2dec('14')*ones(1,channels)]']; % Period Value 
            if channels == 1 %EL5151
               if selection{2} ~= hex2dec('00100000')
	         Excludes=[Excludes;hex2dec('1A03')];
	      end
	    end
    end
  
    config.IO.SlaveConfig = genSlaveConfig(device);
    
    if channels == 1 %EL5151
        if selection{2} ~= hex2dec('00100000')    
            Excludes = [Excludes;hex2dec('1A04') ];
            Excludes = [Excludes; hex2dec('1A05') ];
        end
    end
    %Reset Excludes because of problems in the ec_slave2 S-Function
    Excludes = [];
    config.IO.Pdo.Exclude = Excludes';
    
    %Fix missing Sync manager Information
    for i=1:length(config.SlaveConfig.Descriptions.Devices.Device.TxPdo)
        if config.SlaveConfig.Descriptions.Devices.Device.TxPdo(i).Index == hex2dec('1a03')
            config.SlaveConfig.Descriptions.Devices.Device.TxPdo(i).Sm = 3;
        end
    end

    % Distribute each row of output_pdo_map to output(x).pdo_map
    Pdosize = size(PdoEntries);
    if Pdosize(1) == 1
        config.IO.Port(1).PdoEntry = PdoEntries;
    else
        config.IO.Port = cell2struct(mat2cell(PdoEntries,ones(1,length(PdoEntries)),2),'PdoEntry',2);
    end
    
    
    if selection{2} ~= hex2dec('00100000')
        %Set Index of DC Mode or set [Index AssignActivate cycletimesync0 factor cycletimeshift0 input cycletimesync1 factor cylcetimeshift1]
        if dcmode == 4 % Customized parameter
           config.IO.DcOpMode = dccustom;  
        else
           config.IO.DcOpMode = dcmode-1; 
        end
    end
    config.SdoConfig = cell2struct(...
    num2cell(SdoConfig),...
    {'Index' 'SubIndex' 'BitLen' 'Value'}, 2);

end
