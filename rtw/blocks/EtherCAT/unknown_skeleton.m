function output = unknown_skeleton(command,varargin)
    % General configuration function example for an EtherCAT Slave Block
    % 
    % Please note:
    % The Block must have the hidden Parameter UpdateFcn
    % The parameter is set via set_parameter(gcb,'UpdateFcn','unknown_skeleton')
    % After that the Lib has to be saved
    %
    % The update Function is called by the UpdateLib Block in the etherlab_lib
    % The Update Function updates the menue entries for model selection based on the models described in this m-file
    % Note: only devellopers need to call this functions
    %
    % Example for Init Code of the Simulink Block
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    %masterStr = '';
    %positionStr = '';
    %[Address masterStr positionStr] = getEtherCATAddress(gcb,master,index);
    %selection = unknown_skeleton('getmodel',model);
    %if isempty(selection)
    %  return;
    %end
    %func = selection{3};
    %device = unknown_skeleton('loaddescription',selection);
    %if isempty(device)
    %  return; 
    %end
    %%Prepare S-Funtion structure
    %config = unknown_skeleton('process',selection,device,type_1,type_2,type_3,type_4,dcmode,dccustomer,output_type,scale,offset,dtype,status,filter,tau);
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    %
    % First build up struct with model information
    % Contents: Modelstring, Productcode, Versioncode, ...
    % Functiondescription, Name of EtherCatInfo File, Number of
    % Blockoutputs, Number of Blockinputs
    models = struct(...
        'EP31740002r00100002'          ,{{hex2dec('c664052'), hex2dec('00100002'), 'differential', 'EtherCATInfo_ep3xxx', 4, 0 }}, ...
        'EP31740002r00110002'          ,{{hex2dec('c664052'), hex2dec('00110002'), 'differential', 'EtherCATInfo_ep3xxx', 4, 0 }}, ...
        'EP31740002r00120002'          ,{{hex2dec('c664052'), hex2dec('00120002'), 'differential', 'EtherCATInfo_ep3xxx', 4, 0 }}, ...    
        'EP31821002r001103ea'          ,{{hex2dec('c6e4052'), hex2dec('001103ea'), 'single ended', 'EtherCATInfo_ep3xxx', 2, 0 }}, ...
        'EP31821002r001203ea'          ,{{hex2dec('c6e4052'), hex2dec('001203ea'), 'single ended', 'EtherCATInfo_ep3xxx', 2, 0 }}, ...
        'EP31840002r00100002'          ,{{hex2dec('C704052'), hex2dec('00100002'), 'single ended', 'EtherCATInfo_ep3xxx', 4, 0 }}, ...
        'EP31840002r00110002'          ,{{hex2dec('c704052'), hex2dec('00110002'), 'single ended', 'EtherCATInfo_ep3xxx', 4 ,0 }}, ...
        'EP31841002r001003EA'          ,{{hex2dec('c704052'), hex2dec('001003EA'), 'single ended', 'EtherCATInfo_ep3xxx', 4 ,0 }}, ...
        'EP31841002r001103EA'          ,{{hex2dec('c704052'), hex2dec('001103EA'), 'single ended', 'EtherCATInfo_ep3xxx', 4, 0 }}, ...
        'EP31841002r001203ea'          ,{{hex2dec('c704052'), hex2dec('001203ea'), 'single ended', 'EtherCATInfo_ep3xxx', 4, 0 }} ...  
    );
    %do what is requested by command argument
    switch command
        case 'update'
            % Requested by Update Library Block, do not change this
            set_modelname(models,varargin{1});
            output = [];
        case 'getmodel'
            % Requested by Init Function of the Block Mask,
            % do not change this
            % Checks if requested Model exists in the model struct array
            % and returns the specific contents
            if isfield(models,varargin{1})
                output = models.(varargin{1}); 
            else
                output = [];
                errordlg([gcb ': Slave model "' varargin{1} '" is not implemented'])
            end
        case 'loaddescription'
            % Requested by Init Function of the Block Mask,
            % do not change this
            % Load the EtherCatInfo File and get the device information
            % Return the device information
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
            % Requested by Init function of the Block mask
            % parameter depends on the block parameter in mask
            % all parameter are passed to the process function
            % returns the complete slaveconfig which is the input argument
            % of the ec_slave S-Function
            % Parameterlist for this example (Beckhoff EP3xxx):
            %                       selection  , device    , type_1     ,
            %                       type_2     , type_3    , type_4     ,
            %                       dcmode     , dccustomer, output_type, scale     ,
            %                       offset     , dtype     , status     ,
            %                       filter     , tau
            output = prepare_config(varargin{1},varargin{2},varargin{3},...
                                    varargin{4},varargin{5},varargin{6},...
                                    varargin{7},varargin{8},varargin{9},...
                                    varargin{10},varargin{11},varargin{12},...
                                    varargin{13},varargin{14},varargin{15});
        case 'model'
            % Callback of Block Mask gui parameter of the Block mask
            % optional function
            % switch block GUI Elements on or off depending of selected
            % model
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
       case 'output_type'
            % Callback of Block Mask gui parameter of the Block mask
            % optional function
            % for multi input / output blocks, switching between scalar or
            % vector IO
            %
            % When not in vector output, no control and status outputs
            en = cell2struct(...
                   get_param(gcb,'MaskEnables'),...
                   get_param(gcb,'MaskNames')...
                 );
            values = cell2struct(...
                get_param(gcb,'MaskValues'),...
                get_param(gcb,'MaskNames')...
                );
            if strcmp(values.output_type, 'Vector Output')
               en.status = 'on';
            else
               en.status = 'off';
               set_param(gcb, 'status', 'off');
            end
            set_param(gcb,'MaskEnables',struct2cell(en));
            set_param(gcb,'MaskValues',struct2cell(values));  
            output = [];
        case 'dtype'
            % Callback of Block Mask gui parameter of the Block mask
            % optional function
            % select type of block io for analog io
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
            set_param(gcb,'MaskEnables',struct2cell(en));
            set_param(gcb,'MaskValues',struct2cell(values));  
            output = [];
        case 'filter'
            % Callback of Block Mask gui parameter of the Block mask
            % optional function
            % select filter parameter for analog io
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
    % Update Function
    % do not change!!
     stylestr_model= textscan(get_param(block,'MaskStyleString'),'%s','Delimiter',',');
     names = fieldnames(models);
     namearray = strjoin(names,'|');
     modelstr = strcat('popup(',namearray,')');
     stylestr_model{1}(3) = cellstr(modelstr);
     stylestr_new = strjoin(stylestr_model{1},',');
     set_param(block,'MaskStyleString',stylestr_new);
end

%%
function config = prepare_config(selection,device,type_1,type_2,type_3,type_4, dcmode, dccustomer, output_type,scale, offset,dtype,status,filter,tau)
    % Configuration of slave
    % sets
    % config.IO: IO Configuration of the Simulink block
    % config.SlaveConfig: EtherCAT Config for Slave
    % config.SdoConfig: Sdo Config for Slave
    % config.SoeConfig: SoE Config for Slave
    
    % reset Values
    config.SdoConfig = [];
    config.SoeConfig = [];
    config.IO = [];
    config.SlaveConfig = genSlaveConfig(device);
    SdoConfig = [];
  
    
    %optional do some specific handling depending of number of IOs
    channels = selection{5};    
    %build type array
    basetype=[];
    basetype = [type_1-1; type_2-1; type_3-1; type_4-1];  
    for i=1:channels
        if basetype(i)==3 
            basetype(i) = 6; %0-10V
        end
    end
    
    % do Sdo Configuration
    for i=0:(channels-1)
    SdoConfig = [...
            SdoConfig;...
            hex2dec('F800'), i+1,  16, basetype(i+1);...% Set Input Type
            ]; 
    end 
    
    
    
    %Prepare PDO Mapping
    if status
        StatusPdoEntries = [hex2dec('6000') + hex2dec('10')*(0:(channels-1)); hex2dec('7')*ones(1,channels)]';
        PdoEntries = [hex2dec('6000') + hex2dec('10')*(0:(channels-1)); hex2dec('11')*ones(1,channels)]';
    else
        PdoEntries = [hex2dec('6000') + hex2dec('10')*(0:(channels-1)); hex2dec('11')*ones(1,channels)]';
    end
    
    %optional: do some PDO-Excludes
    config.IO.Pdo.Exclude = [hex2dec('1A01') + 2*(0:(channels-1))];

    %generate static Slaveconfig depending on Slavedescription File
    config.IO.SlaveConfig = genSlaveConfig(device);
    
    
    %Do Simulink Block IO Configuration
    if output_type == 1
        % Vector output chosen

        config.IO.Port(1).PdoEntry = PdoEntries;

        if status
            config.IO.Port(2).PdoEntry = StatusPdoEntries;
        end

        if dtype ~= 1
             config.IO.Port(1).PdoFullScale = 2^15;

            if dtype == 3
                config.IO.Port(1).Gain = scale;
                config.IO.Port(1).Offset = offset;
                config.IO.Port(1).GainName = 'FullScale';
                config.IO.Port(1).OffsetName = 'Offset';
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

    else
        % Separate outputs chosen
        % Distribute each row of output_pdo_map to output(x).pdo_map
        config.IO.Port = cell2struct(mat2cell(PdoEntries,ones(1,channels),2),'PdoEntry',2);
        if status
            config.IO.Port = [output.Port; ...
                cell2struct(mat2cell(StatusPdoEntries,ones(1,channels),2),'PdoEntry',2)];
        end

        if dtype ~= 1
            for i = 1:channels
                config.IO.Port(i).PdoFullScale = 2^15;
            end

            if dtype == 3
                if ~isempty(scale)
                    for i = 1:channels
                        % Choose scale(1) for every output if scale
                        % is a scalar
                        config.IO.Port(i).Gain = scale(min(i,max(size(scale))));
                        config.IO.Port(i).GainName = ['FullScale' num2str(i)];
                    end
                end

                if ~isempty(offset)
                    for i = 1:channels
                        % Choose offset(1) for every output if offset
                        % is a scalar
                        config.IO.Port(i).Offset = offset(min(i,max(size(offset))));
                        config.IO.Port(i).OffsetName = ['Offset' num2str(i)];
                    end
                end
            end

            if filter && ~isempty(tau)
                if sum(tau == 0)
                    errordlg([gcb ': Specify a nonzero time constant '...
                        'for the output filter.']);
                end

                if tsample(1)
                    k = tsample(1)./tau;
                    prefix = 'InputWeight';
                else
                    k = 2*pi./tau;
                    prefix = 'Omega';
                end
                for i = 1:channels
                    % Choose k(1) for every output if k
                    % is a scalar
                    config.IO.Port(i).Filter = k(min(i,max(size(k))));
                    config.IO.Port(i).FilterName = [prefix num2str(i)];
                end

            end
        end
    end  
    
    
    %Configure the DC conofiguration
    if dcmode == 4 % DC Customer
        config.IO.DcOpMode = dccustomer;
    else
        %Set Index of DC Mode or set [Index AssignActivate cycletimesync0 factor0 cycletimeshift0 input0 cycletimesync1 factor1 cylcetimeshit1]
        config.IO.DcOpMode = dcmode-1; 
    end
    
    %finalice the Sdo Configuration, convert to cell array
    config.SdoConfig = cell2struct(...
    num2cell(SdoConfig),...
    {'Index' 'SubIndex' 'BitLen' 'Value'}, 2);

end
