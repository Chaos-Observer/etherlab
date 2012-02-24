function output = beckhoff_ep4xxx(command,varargin)
    models = struct(...
      'EP41740002r00100002'          ,{{hex2dec('104e4052'), hex2dec('00100002'), 'differential', 'EtherCATInfo_ep4xxx', 0, 4 }}, ...
      'EP41740002r00110002'          ,{{hex2dec('104e4052'), hex2dec('00110002'), 'differential', 'EtherCATInfo_ep4xxx', 0, 4 }} ... 
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
            %                       selection  , device    , type_1     ,
            %                       type_2     , type_3    , type_4     ,
            %                       dcmode     , dccustom  ,input_type  ,
            %                       scale
            output = prepare_config(varargin{1},varargin{2},varargin{3},...
                                    varargin{4},varargin{5},varargin{6},...
                                    varargin{7},varargin{8},varargin{9},...
                                    varargin{10});
        case 'model'
            en = cell2struct(...
                get_param(gcb,'MaskEnables'),...
                get_param(gcb,'MaskNames')...
                );
            values = cell2struct(...
                get_param(gcb,'MaskValues'),...
                get_param(gcb,'MaskNames')...
                );
            % 2 or 4 channel terminal
            if str2double(values.model(6)) == 2
                en.type_1='on';
                en.type_2='on';
                en.type_3='off';
                en.type_4='off';
            end
            if str2double(values.model(6)) == 4
                en.type_1='on';
                en.type_2='on';
                en.type_3='on';
                en.type_4='on';
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
function config = prepare_config(selection,device,type_1,type_2,type_3,type_4, dcmode, dccustom, input_type,scale)
    
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
            hex2dec('8000')+hex2dec('10')*i, hex2dec('2'),  8, 0;...% signed representation
            hex2dec('8000')+hex2dec('10')*i, hex2dec('13'),  16, 0;...% default value
            hex2dec('F800'),i+1, 16, basetype(i+1);...% Set Input Type
            ]; 
    end 
    
    
    %output.Pdo.Exclude = [hex2dec('1A01') + 2*(0:(channels-1))];
    
    
    if input_type == 1
        PdoEntryMap = getEntries(device, 'GroupType', 'All');
        config.IO.Port = cell2struct(PdoEntryMap,'PdoEntry',1);
        config.IO.Port.PdoFullScale = 2^15;
        if ~isempty(scale)
            config.IO.Port.Gain = scale;
            config.IO.Port.GainName = 'FullScale';
        end
    elseif input_type == 2
        PdoEntryMap = getEntries(device);
        fs = repmat({2^15},1,channels);
        config.IO.Port = struct('PdoEntry', PdoEntryMap, 'PdoFullScale', 2^15);
        if ~isempty(scale)
            for i = 1:channels
                config.IO.Port(i).Gain = scale(min(i,max(size(scale))));
                config.IO.Port(i).GainName = ['FullScale' num2str(i)];
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