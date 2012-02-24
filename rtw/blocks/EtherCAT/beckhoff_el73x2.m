function output = beckhoff_el73x2(command,varargin)
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
		    'EL7332r00100000'          ,{{hex2dec('1ca43052'), hex2dec('00100000'), '2CH. DC Motor Amplifier', 'EtherCATInfo_el7xxx', 0, 0 }}, ...
		    'EL7332r00110000'          ,{{hex2dec('1ca43052'), hex2dec('00110000'), '2CH. DC Motor Amplifier', 'EtherCATInfo_el7xxx', 0, 0 }} ...
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
            output = prepare_config(varargin{:});
        case 'model'
            % Callback of Block Mask gui parameter of the Block mask
            % optional function
            % switch block GUI Elements on or off depending of selected
            % model
            %en = cell2struct(...
            %    get_param(gcb,'MaskEnables'),...
            %    get_param(gcb,'MaskNames')...
            %    );
            %values = cell2struct(...
            %    get_param(gcb,'MaskValues'),...
            %    get_param(gcb,'MaskNames')...
            %    ); 
            %nothing to do
            %set_param(gcb,'MaskEnables',struct2cell(en));
            %set_param(gcb,'MaskValues',struct2cell(values));
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
max_current= varargin{3};
nom_current= varargin{4};
nom_volt= varargin{5};
resistance= varargin{6};
red_current_pos= varargin{7};
red_current_neg= varargin{8};
kp= varargin{9};
ki= varargin{10};
voltage_adjust_ch1= varargin{11};
current_adjust_ch1= varargin{12};
op_mode_ch1= varargin{13};
inv_dir_ch1= varargin{14};
datafield1_ch1= varargin{15};
datafield2_ch1= varargin{16};
inv_input1_ch1= varargin{17};
inv_input2_ch1= varargin{18};
mode_input1_ch1= varargin{19};
mode_input2_ch1= varargin{20};
voltage_adjust_ch2= varargin{21};
current_adjust_ch2= varargin{22};
op_mode_ch2= varargin{23};
inv_dir_ch2= varargin{24};
datafield1_ch2= varargin{25};
datafield2_ch2= varargin{26};
inv_input1_ch2= varargin{27};
inv_input2_ch2= varargin{28};
mode_input1_ch2= varargin{29};
mode_input2_ch2= varargin{30};
show_digital_inputs= varargin{31};
show_input_channels= varargin{32};
dcmode= varargin{33};
dccustomer= varargin{34};
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


    time_switchoff = [200 200]; % default values in ms
    time_current_lowering = [2000 2000]; % default in ms
    torque_auto_reduction_pos = [0 0]; % default in percent
    torque_auto_reduction_neg = [0 0]; % default in percent
    inner_window = [0 0]; %default in percent
    outer_window = [0 0]; % default in percent
    filter_cutoff_freq = [0 0]; % default in Hz
    op_mode = [op_mode_ch1 op_mode_ch2];
    inv_dir = [inv_dir_ch1 inv_dir_ch2];
    datafield1 = [datafield1_ch1 datafield1_ch2];
    datafield2 = [datafield2_ch1 datafield2_ch2];
    inv_input1 = [inv_input1_ch1 inv_input1_ch2];
    inv_input2 = [inv_input2_ch1 inv_input2_ch2];
    mode_input1 = [mode_input1_ch1 mode_input1_ch2];
    mode_input2 = [mode_input2_ch1 mode_input2_ch2];
    torque_error_enable = [0 0];
    torque_auto_reduce =[0 0];
    voltage_adjust  = [ voltage_adjust_ch1 voltage_adjust_ch2];
    current_adjust = [current_adjust_ch1 current_adjust_ch2];

    % do Sdo Configuration
    for i=0:1
	    SdoConfig = [...
		     SdoConfig;...
			      hex2dec('8000')+(i*hex2dec('10')), 1,  16, max_current(i+1);...% max Current
			      hex2dec('8000')+(i*hex2dec('10')), 2,  16, nom_current(i+1);...% nom Current
			      hex2dec('8000')+(i*hex2dec('10')), 3,  16, nom_volt(i+1);...     % nom Voltage
			      hex2dec('8000')+(i*hex2dec('10')), 4,  16, resistance(i+1);...   % Motor Resistance
			      hex2dec('8000')+(i*hex2dec('10')), 5,  16, red_current_pos(i+1);...% Red Current pos 
			      hex2dec('8000')+(i*hex2dec('10')), 6,  16, red_current_neg(i+1);...% Red Current neg
   			      hex2dec('8000')+(i*hex2dec('10')), hex2dec('C'),  16,  time_switchoff(i+1);...% Time Switchoff 
   			      hex2dec('8000')+(i*hex2dec('10')), hex2dec('D'),  16,  time_current_lowering(i+1);...% Time Current lowering
   			      hex2dec('8000')+(i*hex2dec('10')), hex2dec('E'),  8,  torque_auto_reduction_pos(i+1);...% Torque auto reduction pos
   			      hex2dec('8000')+(i*hex2dec('10')), hex2dec('F'),  8,  torque_auto_reduction_neg(i+1);...% Torque auto reduction neg
			      hex2dec('8001')+(i*hex2dec('10')), 1,  16, kp(i+1);...% kp
			      hex2dec('8001')+(i*hex2dec('10')), 2,  16, ki(i+1);...% ki
			      hex2dec('8001')+(i*hex2dec('10')), 3,  8, inner_window(i+1);...% inner window
			      hex2dec('8001')+(i*hex2dec('10')), 5,  8, outer_window(i+1);...% outer_window
			      hex2dec('8001')+(i*hex2dec('10')), 6,  16, filter_cutoff_freq(i+1);...% Filter cutoff Frequency
			      hex2dec('8001')+(i*hex2dec('10')), hex2dec('11'),  8, voltage_adjust(i+1)==1;...% Voltage adjust Enable
			      hex2dec('8001')+(i*hex2dec('10')), hex2dec('12'),  8, current_adjust(i+1)==1;...% Current adjust Enable
			      hex2dec('8002')+(i*hex2dec('10')), 1,  8, op_mode(i+1)-1;...% Operating Mode
			      hex2dec('8002')+(i*hex2dec('10')), 9,  8, inv_dir(i+1)==1;...% Invert Direction
			      hex2dec('8002')+(i*hex2dec('10')), hex2dec('A'),  8, torque_error_enable(i+1)==1;...% Torque error Enable
			      hex2dec('8002')+(i*hex2dec('10')), hex2dec('B'),  8, torque_auto_reduce(i+1)==1;...% Torque Auto reduce
			      hex2dec('8002')+(i*hex2dec('10')), hex2dec('11'),  8, datafield1(i+1);...% Data Info field 1
			      hex2dec('8002')+(i*hex2dec('10')), hex2dec('19'),  8, datafield2(i+1);...% Data Info field 2
			      hex2dec('8002')+(i*hex2dec('10')), hex2dec('30'),  8, inv_input1(i+1);...% Invert Digital Input1
			      hex2dec('8002')+(i*hex2dec('10')), hex2dec('31'),  8, inv_input2(i+1);...% Invert Digital Input1
			      hex2dec('8002')+(i*hex2dec('10')), hex2dec('32'),  8, mode_input1(i+1)-1;...% Mode Digital Input 1
			      hex2dec('8002')+(i*hex2dec('10')), hex2dec('33'),  8, mode_input2(i+1)-1;...% Mode Digital Input 2
			 ]; 
    end 
    
    %Prepare PDO Mapping    
    PdoEntries = [];
    index_digital_inputs = 0;
    index_input_channels = 0;
    PdoEntries = [PdoEntries;[hex2dec('6000') + hex2dec('10')*(0:1); hex2dec('1')*ones(1,2)]']; %ready to enable
    PdoEntries = [PdoEntries;[hex2dec('6000') + hex2dec('10')*(0:1); hex2dec('2')*ones(1,2)]']; %ready
    PdoEntries = [PdoEntries;[hex2dec('6000') + hex2dec('10')*(0:1); hex2dec('3')*ones(1,2)]']; %warning
    PdoEntries = [PdoEntries;[hex2dec('6000') + hex2dec('10')*(0:1); hex2dec('4')*ones(1,2)]']; %error
    if show_digital_inputs
        PdoEntries = [PdoEntries;[hex2dec('6000') + hex2dec('10')*(0:1); hex2dec('C')*ones(1,2)]']; %Digital Input 1        
        index_digital_inputs = max(size(PdoEntries))/2;
        PdoEntries = [PdoEntries;[hex2dec('6000') + hex2dec('10')*(0:1); hex2dec('D')*ones(1,2)]']; %Digital Input 2        
    end
    if show_input_channels
        PdoEntries = [PdoEntries;[hex2dec('6000') + hex2dec('10')*(0:1); hex2dec('11')*ones(1,2)]']; %Info Channel1                
        index_input_channels = max(size(PdoEntries))/2;
        PdoEntries = [PdoEntries;[hex2dec('6000') + hex2dec('10')*(0:1); hex2dec('12')*ones(1,2)]']; %Info Channel2                
    end
    
    
    
    %Set Input Channels
    PdoEntries = [PdoEntries;[hex2dec('7000') + hex2dec('10')*(0:1); hex2dec('1')*ones(1,2)]']; %enable                
    PdoEntries = [PdoEntries;[hex2dec('7000') + hex2dec('10')*(0:1); hex2dec('2')*ones(1,2)]']; %reset     
    PdoEntries = [PdoEntries;[hex2dec('7000') + hex2dec('10')*(0:1); hex2dec('3')*ones(1,2)]']; %reduce torque                
    PdoEntries = [PdoEntries;[hex2dec('7000') + hex2dec('10')*(0:1); hex2dec('21')*ones(1,2)]']; %SetPount                

    

    
    %optional: do some PDO-Excludes
    %config.IO.Pdo.Exclude = [hex2dec('1A01') + 2*(0:(channels-1))];

    %generate static Slaveconfig depending on Slavedescription File
    config.IO.SlaveConfig = genSlaveConfig(device);
    %Add Sm Entries for TxPdos
    % 0x1a01
    config.SlaveConfig.Descriptions.Devices.Device.TxPdo(2).Sm = 3;
    % 0x1a03
    config.SlaveConfig.Descriptions.Devices.Device.TxPdo(4).Sm = 3;
    
    for i=1:(max(size(PdoEntries))/2)
        config.IO.Port(i).PdoEntry = PdoEntries((i*2)-1:i*2,:);
    end
    
    %scale and Offset for Setpoint
    index_setpoint = max(size(PdoEntries))/2;
    config.IO.Port(index_setpoint).PdoFullScale = 2^15;
    config.IO.Port(index_setpoint).Gain = nom_current;
    config.IO.Port(index_setpoint).Offset = [0 0];
    
    
    %Configure the DC conofiguration
    if dcmode == 3 % DC Customer
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
