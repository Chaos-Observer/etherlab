function output = esd_cangateway(command,varargin)
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
        'CANr00000000'          ,{{hex2dec('00000002'), hex2dec('00000000'), 'CAN Gateway', 'EtherCATInfo_esdcan', 0, 0 }} ...
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
	  output = prepare_config(varargin{:});
        case 'model'
            % Callback of Block Mask gui parameter of the Block mask
            % optional function
            % switch block GUI Elements on or off depending of selected
            % model
            output = [];
        case 'dc'
            %en = cell2struct(...
            %    get_param(gcb,'MaskEnables'),...
            %    get_param(gcb,'MaskNames')...
            %    );
            %values = cell2struct(...
            %    get_param(gcb,'MaskValues'),...
            %    get_param(gcb,'MaskNames')...
            %    );
            %if strcmp(values.dcmode, 'DC-Customized');
            %    en.dccustom = 'on';
            %else
            %    en.dccustom = 'off';
            %end;
            %set_param(gcb,'MaskEnables',struct2cell(en));
            %set_param(gcb,'MaskValues',struct2cell(values));                                              
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
function config = prepare_config(varargin)

  selection = varargin{1};
  device = varargin{2};
  numberrx = varargin{3};
  numbertx = varargin{4};
  isext = varargin{5};
  baudrate = varargin{6};
  rxfilter = varargin{7};
  nodeadr = varargin{8};

    % reset Values
    config.SdoConfig = [];
    config.SoeConfig = [];
    config.IO = [];
    config.SlaveConfig = genSlaveConfig(device);
    SdoConfig = [];
  
    
     
    % do Sdo Configuration

    SdoConfig = [...
            SdoConfig;...
		     hex2dec('8000'), hex2dec('1'),  16, nodeadr;...     % Node Adress
		     hex2dec('8000'), hex2dec('20'), 16, isext*8;...     % 29 or 11 Bit adress range
		     hex2dec('8000'), hex2dec('21'),  8, numberrx;...    % Number RX Frames
		     hex2dec('8000'), hex2dec('22'),  8, numbertx;...    % Number TX Frames
		     hex2dec('F800'), hex2dec('2'),   8, baudrate-1;...  % Baudrate 
            ]; 
    

    %Build sdo array to write RXFilterTable
    numberoffilterid = numel(rxfilter); %Get number of filter entries
    if isext==1
        rxfilter = rxfilter(:)+2^32; %Set Bit 31
    end
    rxfilterhex = dec2base(uint32(rxfilter),16,8); % Convert to uint 32, swap to little endian an create char array
    sdoarray = uint8([]);
    sdoarray(1) = uint32(numberoffilterid)/2; %Number of Sdo Entries %Note it is a uint64 value sdo
    for i=1:numberoffilterid
	    sdoarray = [sdoarray; ...
				 uint8(sscanf(rxfilterhex(i,:),'%2x'))
			];
    end

    %Extend if needed to full 64 Bit
    if mod(numberoffilterid,2)==1
      sdoarray = [sdoarray; ...
	 uint8(sscanf('00000000','%2x'))
		  ];    
    end
   

    %generate static Slaveconfig depending on Slavedescription File
    config.IO.SlaveConfig = genSlaveConfig(device);
   
    %Define the PDO Base
    %if isext == 1
    %   rxbase = hex2dec('6001');
    %   txbase = hex2dec('7001');
    %else
       rxbase = hex2dec('6000');
       txbase = hex2dec('7000');
    %end
    
    %Prepare Pdo Mapping
    %First Control Outputs
    PdoEntriesConfInput = [rxbase * ones(1, 4); (1:4)]';
    config.IO.Port(1).PdoEntry = PdoEntriesConfInput;
    %Next Control Inputs
    PdoEntriesConfInput = [txbase * ones(1, 3); (1:3)]';
    config.IO.Port(2).PdoEntry =  PdoEntriesConfInput;
    %Next TxFrames
    PdoFramesTX =  [txbase * ones(1, numbertx); 4+(0:numbertx-1)]';
    config.IO.Port(3).PdoEntry =  PdoFramesTX;
    %Next RxFrames
    PdoFramesRX =  [rxbase * ones(1, numberrx); 5+(0:numberrx-1)]';
    config.IO.Port(4).PdoEntry = PdoFramesRX; 
      
    %finalice the Sdo Configuration, convert to cell array
    config.SdoConfig = cell2struct(...
    num2cell(SdoConfig),...
    {'Index' 'SubIndex' 'BitLen' 'Value'}, 2);

    %Add Bytearray Entry
    if numberoffilterid > 0
      config.SdoConfig(end+1).Index = hex2dec('8001');
      config.SdoConfig(end).ByteArray =  sdoarray;
    end
end
