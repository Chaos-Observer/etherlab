function output = ep3xxx(command,varargin)

switch command
	case 'update'
		set_modelname(models,varargin{1});
		output = [];
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
   case 'output_type'
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
    case 'process'
	    output = prepare_config(varargin{:}); 
	otherwise
		disp('Unknown method.')
		output = [];  
end
 


function rv = prepare_config(model,output_type, status, dtype, scale, offset, filter,...
                     tau, type_1,  type_2, type_3, type_4, dcmode, dccustomer)

entries = [...
           hex2dec('6000'),  7, 8, 2008; ...
           hex2dec('6010'),  7, 8, 2008; ...
           hex2dec('6020'),  7, 8, 2008; ...
           hex2dec('6030'),  7, 8, 2008; ...
           
           hex2dec('6000'), 17, 16, 1001; ...
           hex2dec('6010'), 17, 16, 1001; ...
           hex2dec('6020'), 17, 16, 1001; ...
           hex2dec('6030'), 17, 16, 1001; ...
           
           hex2dec('7020'),  1,  1, 2016; ...
           hex2dec('7020'),  2,  1, 2016; ...
          ];

        
 %                   TxPdoSt TxPdoEnd StatusStart StatusEnd                          
pdo = [...               
        hex2dec('1a01'),  5,    5 ;...
        hex2dec('1a03'),  6,    6 ;...
        hex2dec('1a05'),  7,    7 ;...
        hex2dec('1a07'),  8,    8 ;...
        hex2dec('1a00'),  1,    1 ;...
        hex2dec('1a02'),  2,    2 ;...                               
        hex2dec('1a04'),  3,    3 ;...
        hex2dec('1a06'),  4,    4 ;...
        
        hex2dec('1a01'),  5,    5 ;...
        hex2dec('1a03'),  6,    6 ;...
        hex2dec('1a00'),  1,    1 ;...
        hex2dec('1a02'),  2,    2 ;...
                            
        hex2dec('1600'),  8,    9 ;...
      ];                                                                    
                                                                            

%   Model       ProductCode          Revision      			TxStart|TxEnd|TxStart | TxEnd | func
                                %		          			  with	 with without  without|
%												   				        Status            |
models = struct(...
	 'EP31740002r00100002' ,[hex2dec('c664052'), hex2dec('00100002'),  1,  8,  1,  4, 1],...  
	 'EP31740002r00110002' ,[hex2dec('c664052'), hex2dec('00110002'),  1,  8,  1,  4, 1],... 
	 'EP31740002r00120002' ,[hex2dec('c664052'), hex2dec('00120002'),  1,  8,  1,  4, 1],...
	 'EP31821002r001103ea' ,[hex2dec('c6e4052'), hex2dec('001103ea'),  9, 12,  9, 10, 2],...
	 'EP31821002r001203ea' ,[hex2dec('c6e4052'), hex2dec('001203ea'),  9, 12,  9, 10, 2],...
	 'EP31840002r00100002' ,[hex2dec('C704052'), hex2dec('00100002'),  1,  8,  1,  4, 2],...
	 'EP31840002r00110002' ,[hex2dec('c704052'), hex2dec('00110002'),  1,  8,  1,  4, 2],...
	 'EP31841002r001003EA' ,[hex2dec('c704052'), hex2dec('001003EA'),  1,  8,  1,  4, 2],...
	 'EP31841002r001103EA' ,[hex2dec('c704052'), hex2dec('001103EA'),  1,  8,  1,  4, 2],...
	 'EP31841002r001203ea' ,[hex2dec('c704052'), hex2dec('001203ea'),  1,  8,  1,  4, 2]...
	   );                      
                

rv.SlaveConfig.vendor = 2;       

product = models.(model);            
rv.SlaveConfig.product = product(1); 
rv.SlaveConfig.description = model; 

% Determine Function Output
func = {'differential', 'single end'};
rv.output_func = func{product(7)};

% Choose required pdos 
if  status 
    pdoindex = product(3):product(4);
else
    pdoindex = product(5):product(6);
end

% Set Number of pdo's and channels
number_pdo = numel(pdoindex);
number_elements = number_pdo;

if status
    number_elements = number_elements/2;
end

% Set the output type for each port
basetype = [type_1-1; type_2-1; type_3-1; type_4-1];  
for i=1:number_elements
    if basetype(i)==3 
        basetype(i) = 6; %0-10V
    end
end

% Configure Sdo's for output types

rv.slaveConfig.sdo = cell(number_elements,4);
for k = 1:number_elements
    rv.SlaveConfig.sdo{k,1} = hex2dec('F800');
    rv.SlaveConfig.sdo{k,2} = k;
    rv.SlaveConfig.sdo{k,3} = 16;
    rv.SlaveConfig.sdo{k,4} = basetype(k);
end    


% set DC mode 
% 
%[AssignActivate, CycleTimeSync0, CycleTimeSync0Factor, ShiftTimeSync0,...
% ShiftTimeSync0Factor, ShiftTimeSync0Input, CycleTimeSync1, CycleTimeSync1Factor,...
% ShiftTimeSync1, ShiftTimeSync1Factor]   

dcstate = [              0, 0, 0, 0, 0, 0, 0, 1,     0, 0;...
            hex2dec('700'), 0, 1, 0, 0, 0, 0, 1, 10000, 0;...
            hex2dec('700'), 0, 1, 0, 0, 1, 0, 1, 10000, 0;...
];


if dcmode == 4 % DC Customer
    rv.SlaveConfig.dc = dccustomer;
else
     %Set Index of DC Mode or set [Index AssignActivate cycletimesync0 factor0 cycletimeshift0 input0 cycletimesync1 factor1 cylcetimeshit1]
    rv.SlaveConfig.dc = dcstate(dcmode,:); 
end
  

% RxPdo SyncManager
rv.SlaveConfig.sm = { {3 1 {}} };


% Populate the RxPDO Inputs structure
rv.SlaveConfig.sm{1}{3} = arrayfun(...
        @(x) {pdo(x,1), entries(pdo(x,2):pdo(x,3),:)}, ...
        pdoindex, ...
        'UniformOutput',0 ...
);


% set scale for double outputs
scale_int = 2^15;


% Set data type scale
if ~strcmp(dtype, 'Raw Bits')
    if strcmp(output_type, 'Separate Outputs')
        for k = 1:number_pdo
            rv.PortConfig.output(k).full_scale = scale_int; 
        end
    else
            rv.PortConfig.output(1).full_scale = scale_int; 
    end
end

% Fill in Offsets
if filter
    if (isempty(tau) || numel(tau)==1 || numel(tau) == number_elements)
        if isempty(find(tau <= 0, 1))   
            if strcmp(output_type,'Separate Outputs')        
                for k = 1:number_elements
                    if numel(tau) == 1
                        rv.PortConfig.output(k).filter = {'Filter', tau};
                    elseif numel(tau) == number_elements 
                        rv.PortConfig.output(k).filter = {'Filter', tau(k)};
                    else
                        rv.PortConfig.output(k).filter = [];
                    end
                end 
            else
                rv.PortConfig.output.filter = {'Filter', tau};
            end
        else
            errodlg(['Specify a nonzero time constant '...
                     'for the output filter'],'Filter Error');
        end
     % if input is wrong, fill with emptys
    else 
        if strcmp(output_type,'Separate Outputs')
            for k = 1:number_elements
                rv.PortConfig.output(k).filter = [];
            end
        else
           rv.PortConfig.output.filter = {'Filter', tau};
        end
           warning('EtherLAB:Beckhoff:EL31xx:filter', ['The dimension of the'...
           ' filter output does not match to the number of elements of the'...
           ' terminal. Please choose a valid output, or the filter is being ignored'])
    end
end


% set scale and offset 
if strcmp(dtype, 'Double with scale and offset')
% Fill in Scale
    if (isempty(scale) || numel(scale)==1 || numel(scale) == number_elements)   
        if strcmp(output_type,'Separate Outputs')        
            for k = 1:number_elements
                if numel(scale) == 1
                    rv.PortConfig.output(k).gain = {'Gain', scale};
                elseif numel(scale) == number_elements 
                    rv.PortConfig.output(k).gain = {'Gain', scale(k)};
                else
                    rv.PortConfig.output(k).gain = {'Gain', []};
                end
             end
        else
            rv.PortConfig.output.gain = {'Gain', scale};
        end
         % if input is wrong, fill with emptys
    else 
        if strcmp(output_type,'Separate Outputs')
            for k = 1:number_elements
                rv.PortConfig.output(k).gain = [];
            end
        else
            rv.PortConfig.output.gain = {'Gain', scale};
        end
        warning('EtherLAB:Beckhoff:EL31xx:scale', ['The dimension of the'...
        ' scale output does not match to the number of elements of the'...
        ' terminal. Please choose a valid output, or the scale is being ignored'])
    end
     
    % Fill in Offsets
    if (isempty(offset) || numel(offset)==1 || numel(offset) == number_elements)   
        if strcmp(output_type,'Separate Outputs')        
            for k = 1:number_elements
                if numel(offset) == 1
                    rv.PortConfig.output(k).offset = {'Offset', offset};
                elseif numel(offset) == number_elements 
                    rv.PortConfig.output(k).offset = {'Offset', offset(k)};
                else
                    rv.PortConfig.output(k).offset = [];
                end
             end
        else
            rv.PortConfig.output.offset = {'Offset', offset};
        end
         % if input is wrong, fill with emptys
    else 
        if strcmp(output_type,'Separate Outputs')
            for k = 1:number_elements
                rv.PortConfig.output(k).offset = [];
            end
        else
            rv.PortConfig.output.offset = {'Offset', offset};
        end
        warning('EtherLAB:Beckhoff:EL31xx:offset', ['The dimension of the'...
        ' offset output does not match to the number of elements of the'...
        ' terminal. Please choose a valid output, or the offset is being ignored'])
    end
end



% Populate the block's output port(s)

r = 0:number_elements-1;
if strcmp(output_type, 'Vector Output')
    if status
        for k = 0:1
        rv.PortConfig.output(k+1).pdo = zeros(numel(r),4);
        rv.PortConfig.output(k+1).pdo(:,2) = number_elements*k+r;
        end
    else
        rv.PortConfig.output.pdo = zeros(numel(r),4);
        rv.PortConfig.output.pdo(:,2) = r;
    end
else
    for k = 1:number_elements
        rv.PortConfig.output(k).pdo = [0, r(k), 0, 0];
    end
end
