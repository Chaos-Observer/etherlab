function rv = el31xx(model,input_type, diagnose, dtype, scale, offset, filter, tau)

%model = 'EL2032v2'
%diagnose = 1;
%input_type = 'Vector Input'

entries = [...
        hex2dec('3101'), 2, 16, 2016; ... %Channel outputs
        hex2dec('3102'), 2, 16, 2016; ...
        hex2dec('3101'), 1,  8, 1008; ... %State outputs
        hex2dec('3102'), 1,  8, 1008; ...
        hex2dec('6401'), 1, 16, 2016; ... %Channel outputs without states
        hex2dec('6401'), 2, 16, 2016; ...
 
        % hex2dec('3101'), 1,  8, 1008; ...
        % hex2dec('3101'), 2, 16, 1016; ... 
        % hex2dec('3102'), 1,  8, 1008; ...
        % hex2dec('3102'), 2, 16, 1016; ...
        % hex2dec('6401'), 1, 16, 1016; ...
        % hex2dec('6401'), 2, 16, 1016; ...       
        ];

        

pdo = [...
        hex2dec('1a00'),  1,  2;...       %TxPdo Sm3 El3102, El3142, EL3152, EL3162
        hex2dec('1a01'),  3,  4;...       %TxPdo Sm3 El3102, El3142, EL3152, EL3162
        hex2dec('1a10'),  5,  6;...       %TxPdo Sm3 El3102, El3142, EL3152, EL3162
  
%        hex2dec('1a00'),  7,  8;...       %TxPdo Sm3 El3142, EL3152, EL3162
%        hex2dec('1a01'),  9, 10;...       %TxPdo Sm3 El3142, EL3152, EL3162
%        hex2dec('1a10'), 11, 12;...       %TxPdo Sm3 El3142, EL3152, EL3162
 
      ];

%   Model       ProductCode          Revision       TxStart|TxEnd|TxStart | TxEnd
%                                                     with   with without  without
%                                                              State
models = struct(...
  'EL3102'  ,[hex2dec('0C1E3052'), hex2dec('00000000'), 1,    4,     5,    ,6  }},...
  'EL3142'  ,[hex2dec('0C463052'), hex2dec('00000000'), 1,    4,     5,    ,6  }},...
  'EL3152'  ,[hex2dec('0C503052'), hex2dec('00000000'), 1,    4,     5,    ,6  }},...
  'EL3162'  ,[hex2dec('0C5A3052'), hex2dec('00000000'), 1,    4,     5,    ,6  }},...
    );

rv.SlaveConfig.vendor = 2;

product = models.(model);
rv.SlaveConfig.product = product(1);

% RxPdo SyncManager
rv.SlaveConfig.pdo = { {3 1 {}} };

% Choose required pdos 
if  state 
    pdoindex = product(3):product(4)
else
    pdoindex = product(5):product(6)
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
rv.PortConfig.input = struct(...
    'gain', [],... 
    'offset', [],...
    'filter', [],...
    'full_scale', [],...
    );

% set scale and offset 
if dtype = 'Double with scale and offset'
    % Check vor valiability of variable inputs
    if isempty(scale) || numel(scale)==1 || numel(scale) = number_elements
        rv.Portconfig.input.scale = {'Scale',scale};
    else
        warning('EtherLAB:Beckhoff:EL31xx:scale', ['The dimension of the'...
        ' scale input does not match to the number of elements of the'...
        ' terminal. Please choose a valid input, or the scale is being ignored'])
    end
    if isempty(offset) || numel(offset)==1 || numel(offset) = number_elements
        rv.Portconfig.input.offset = {'Offset', offset);
    else
        warning('EtherLAB:Beckhoff:EL31xx:offset', ['The dimension of the'...
       ' offset input does not match to the number of elements of the'...
       ' terminal. Please choose a valid input, or the offset is being ignored'])
    end 
end

% Set data type scale
if data_type ~= 'Raw Bits'
    rv.Portconfig.input.full_scale = scale_int; 
end

% define filter if choosen
if filter && dtype ~= 'Raw bits' 
    if numel(filter) == 1
        if filter < 0
            errodlg('Please choose a positive time constant','Filter Error');
        else
            rv.Portconfig.input.filter = {'Filter',filter};
        end
   else
       warning('EtherLAB:Beckhoff:EL31xx:filter', ['Filter input must be a'...
               ' scalar. Please choose a valid input, or the filter is being'...
               ' ignored');    
   end
end

% Set input port







% Populate the block's output port(s)
r = 0:(product(4) - product(3));
if strcmp(input_type, 'Vector Input')
    rv.PortConfig.input.pdo = [zeros(numel(r),4)];
    rv.PortConfig.input.pdo(:,2) = r;
else
    rv.PortConfig.input = arrayfun(@(x) struct('pdo', [0, x, 0, 0]), r);
end



% Maybe diagnose TxPdo SyncManager
if product(5)
  rv.SlaveConfig.pdo{2} = {1 1 {}};

% Populate the TxPDO Inputs structure
  rv.SlaveConfig.pdo{2}{3} = arrayfun(...
        @(x) {pdo(x,1), entries(pdo(x,2):pdo(x,3),:)}, ...
        product(5):product(6), ...
        'UniformOutput',0 ...
  );

% If diagnose is enabled and available set outputs
  if  diagnose && product(5)
     r = 0:(product(6) - product(5));
    if strcmp(input_type, 'Vector Input')
      rv.PortConfig.output.pdo = [ones(numel(r),1) zeros(numel(r),3)];
      rv.PortConfig.output.pdo(:,2) = r;
    else
      rv.PortConfig.output = arrayfun(@(x) struct('pdo', [1, x, 0, 0]), r);
    end
  end
end
return