function rv = ep3xxx(model,output_type, status, dtype, scale, offset, filter, tau)

%model = 'EL2032v2'
%diagnose = 1;
%input_type = 'Vector Input'

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
        hex2dec('1a00'),  5,    5,          1,       1 ;...
        hex2dec('1a02'),  6,    6,          2,       2 ;...
        hex2dec('1a04'),  7,    7,          3,       3 ;...
        hex2dec('1a06'),  8,    8,          4,       4 ;...
        hex2dec('1a01'),  5,    5,          0,       0 ;...
        hex2dec('1a03'),  6,    6,          0,       0 ;...
        hex2dec('1a05'),  7,    7,          0,       0 ;...
        hex2dec('1a07'),  8,    8,          0,       0 ;...

        hex2dec('1600'),  8,    9,          0,       0 ;...
      ];                                                                    
                                                                            

%   Model       ProductCode          Revision                        TxStart|TxEnd|TxStart | TxEnd
%                                                                      with   with without  without
%                                                                               Status
models = struct(...
     'EP31740002r00100002' ,[hex2dec('c664052'), hex2dec('00100002'),    1,    4,     5,     8],...  
     'EP31740002r00110002' ,[hex2dec('c664052'), hex2dec('00110002'), 	 1,	   4,	  5,	 8],... 
     'EP31740002r00120002' ,[hex2dec('c664052'), hex2dec('00120002'),	 1,	   4,	  5,	 8],...
	 'EP31821002r001103ea' ,[hex2dec('c6e4052'), hex2dec('001103ea'),    1,    2,     5.     6],...
	 'EP31821002r001203ea' ,[hex2dec('c6e4052'), hex2dec('001203ea'),    1,    2,     5.     6],...
	 'EP31840002r00100002' ,[hex2dec('C704052'), hex2dec('00100002'),	 1,	   4,	  5,	 8],...
	 'EP31840002r00110002' ,[hex2dec('c704052'), hex2dec('00110002'),	 1,	   4,	  5,	 8],...
	 'EP31841002r001003EA' ,[hex2dec('c704052'), hex2dec('001003EA'), 	 1,	   4,	  5,	 8],...
	 'EP31841002r001103EA' ,[hex2dec('c704052'), hex2dec('001103EA'),  	 1,	   4,	  5,	 8],...
	 'EP31841002r001203ea' ,[hex2dec('c704052'), hex2dec('001203ea'),	 1,	   4,	  5,	 8]...
	   );

 
 

%'differential', ...
%'differential', ...
%'differential', ...
%'single ended', ...
%'single ended', ...
%'single ended', ...
%'single ended', ...
%'single ended', ...
%'single ended', ...
%'single ended' ... 


    
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
	otherwise
		disp('Unknown method.')
		output = [];  
end

rv.SlaveConfig.vendor = 2;       

                             
product = models.(model);            
rv.SlaveConfig.product = product(1); 


% set output types
basetype = [type_1-1; type_2-1; type_3-1; type_4-1];  
for i=1:channels
    if basetype(i)==3 
        basetype(i) = 6; %0-10V
    end
end

% Configure Sdo's for output types
rv.SlaveConfig.sdo = arrayfun(...
          @(x) {hex2dec('F800'), x+1, 16, basetype(x+1)},...
          0:(product(6)-product(5)),...
          'UniformOutput',0 ... 
);

% RxPdo SyncManager
rv.SlaveConfig.pdo = { {3 1 {}} };

% Choose required pdos 
if  status 
    pdoindex = product(3):product(4);
else
    pdoindex = product(5):product(6);
end

% Populate the RxPDO Inputs structure
rv.SlaveConfig.pdo{1}{3} = arrayfun(...
        @(x) {pdo(x,1), entries(pdo(x,2):pdo(x,3),:)}, ...
        pdoindex, ...
        'UniformOutput',0 ...
);

% set scale for double outputs
scale_int = 2^15;
number_elements = 2;

% Set scale and offset for each data type
rv.PortConfig.output = struct(...
    'gain', [],... 
    'offset', [],...
    'filter', [],...
    'full_scale', []...
    );


% set scale and offset 
if strcmp(dtype, 'Double with scale and offset')
    % Check vor valiability of variable outputs
    % Set Scales
    if  strcmp(output_type,'Separate Outputs') &&...
            (isempty(scale) || numel(scale)==1 || numel(scale) == number_elements)
            if numel(scale) == 1
                rv.PortConfig.output.scale = ...
                    {'Scale', [ones(1,number_elements)]*scale};
            else
                rv.PortConfig.output.scale = {'Scale', scale};
            end
     elseif strcmp(output_type,'Vector Output') &&...
             (isempty(scale) || numel(scale)==1)
              rv.PortConfig.output.scale = {'Scale', scale};
     else
        warning('EtherLAB:Beckhoff:EL31xx:scale', ['The dimension of the'...
       ' scale output does not match to the number of elements of the'...
       ' terminal. Please choose a valid output, or the scale is being ignored'])
     end
    % Set Offsets
    if  strcmp(output_type,'Separate Outputs') &&...
            (isempty(offset) || numel(offset)==1 || numel(offset) == number_elements)
            if numel(offset) == 1
                rv.PortConfig.output.offset = ...
                    {'Offset', [ones(1,number_elements)]*offset};
            else
                rv.PortConfig.output.offset = {'Offset', offset};
            end
     elseif strcmp(output_type,'Vector Output') &&...
             (isempty(offset) || numel(offset)==1)
              rv.PortConfig.output.offset = {'Offset', offset};
     else
        warning('EtherLAB:Beckhoff:EL31xx:offset', ['The dimension of the'...
       ' offset output does not match to the number of elements of the'...
       ' terminal. Please choose a valid output, or the offset is being ignored'])
     end 
end


% Set data type scale
if ~strcmp(dtype, 'Raw Bits')
    rv.PortConfig.output.full_scale = scale_int; 
end


% define filter if choosen
if filter && ~strcmp(dtype, 'Raw bits') 
    if numel(filter) == 1
        if filter < 0
            errodlg(['Specify a nonzero time constant '...
                        'for the output filter'],'Filter Error');
        else
            rv.PortConfig.output.filter = {'Filter', tau};
        end
   else
       warning('EtherLAB:Beckhoff:EL31xx:filter', ['Filter output must be a'...
               ' scalar. Please choose a valid output, or the filter is being'...
               ' ignored']);    
   end
end


% Populate the block's output port(s)
r = 0:1;

if strcmp(output_type, 'Vector Output')
    if status
        for k = 0:1
        rv.PortConfig.output(k+1).pdo = [zeros(numel(r),4)];
        rv.PortConfig.output(k+1).pdo(:,2) = [r];
        rv.PortConfig.output(k+1).pdo(:,3) = k;
        end
    else
        rv.PortConfig.output.pdo = [zeros(numel(r),4)];
        rv.PortConfig.output.pdo(:,3) = [r];
    end
else
    rv.PortConfig.output = arrayfun(@(x) struct('pdo', [0, 0, x, 0]), r);
end


if status && strcmp(output_type, 'Vector Output')
   if ~isempty(offset)
       rv.PortConfig.output(2).offset = {'Offset', [0 0]};;
   end
   if ~isempty(scale)  
       rv.PortConfig.output(2).scale = {'Scale', [1 1]};
   end 
  rv.PortConfig.output(2).pdo = [zeros(numel(r),4)];
 
  rv.PortConfig.output(2).pdo(:,3) = 1;   
  rv.PortConfig.output(2).pdo(:,2) = [0;1];
end

