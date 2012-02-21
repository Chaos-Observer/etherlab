%function output = ep3xxx(command,varargin)
% 
% 
% 
function rv = ep4xxx(model,input_type, scale, type_1, type_2,...
                     type_3, type_4, dcmode, dccustom)

entries = [...
           hex2dec('7000'), 17, 16, 2016; ...
           hex2dec('7010'), 17, 16, 2016; ...
           hex2dec('7020'), 17, 16, 2016; ...
           hex2dec('7030'), 17, 16, 2016; ...
          ];

        
 %                   TxPdoSt TxPdoEnd StatusStart StatusEnd                          
pdo = [...               
        hex2dec('1600'),  1,    1 ;...
        hex2dec('1601'),  2,    2 ;...
        hex2dec('1602'),  3,    3 ;...
        hex2dec('1603'),  4,    4 ...
      ];                                                                    
                                                                            

%   Model                   ProductCode          Revision       TxStart | TxEnd | func
                      
models = struct(...
'EP41740002r00100002' ,[ hex2dec('104e4052'), hex2dec('00100002'),  1,     4,    1], ...
'EP41740002r00110002' ,[ hex2dec('104e4052'), hex2dec('00110002'),  1,     4,    1] ...
    );                      
                

rv.SlaveConfig.vendor = 2;       

product = models.(model);            
rv.SlaveConfig.product = product(1); 

% Determine Function Input
func = {'differential', 'single end'};
rv.input_func = func{product(5)};

% All Pdo's 
pdoindex = product(3):product(4);
number_pdo = numel(pdoindex);


% Set the input type for each port
basetype = [type_1-1; type_2-1; type_3-1; type_4-1];  
for i=1:number_pdo
    if basetype(i)==3 
        basetype(i) = 6; %0-10V
    end
end

% Configure Sdo's for input types

rv,slaveConfig.sdo = cell(number_pdo,4);
for k = 1:number_pdo
    rv.SlaveConfig.sdo{k,1} = hex2dec('F800');
    rv.SlaveConfig.sdo{k,2} = k;
    rv.SlaveConfig.sdo{k,3} = 16;
    rv.SlaveConfig.sdo{k,4} = basetype(k);
end    

% set DC mode 
%                   AA,     CST0, STS0, CST1,  STS1,
dcstate = [              0,   0,    0,    1,       0;...
            hex2dec('730'),   1,    0,    1,  140000;...
];

if dcmode == 3 % DC Customer
    rv.SlaveConfig.dc = dccustom;
else
     %Set Index of DC Mode or set [Index AssignActivate cycletimesync0 factor0 cycletimeshift0 input0 cycletimesync1 factor1 cylcetimeshit1]
    rv.SlaveConfig.dc = dcstate(dcmode,:); 
end
  

% RxPdo SyncManager
rv.SlaveConfig.pdo = { {2 0 {}} };


% Populate the RxPDO Inputs structure
rv.SlaveConfig.pdo{1}{3} = arrayfun(...
        @(x) {pdo(x,1), entries(pdo(x,2):pdo(x,3),:)}, ...
        pdoindex, ...
        'UniformOutput',0 ...
);


% set scale for double inputs
scale_int = 2^15;


if (isempty(scale) || numel(scale)==1 || numel(scale) == number_pdo)   
    if strcmp(input_type,'Separate Outputs')        
        for k = 1:number_pdo
            if numel(scale) == 1
                rv.PortConfig.input(k).gain = {'Gain', scale};
            elseif numel(scale) == number_pdo 
                rv.PortConfig.input(k).gain = {'Gain', scale(k)};
            else
                rv.PortConfig.input(k).gain = {'Gain', []};
            end
         end
    else
        rv.PortConfig.input.gain = {'Gain', scale};
    end
     % if input is wrong, fill with emptys
else 
    if strcmp(input_type,'Separate Outputs')
        for k = 1:number_pdo
            rv.PortConfig.input(k).gain = [];
        end
    else
        rv.PortConfig.input.gain = {'Gain', scale};
    end
    warning('EtherLAB:Beckhoff:EP4xxx:scale', ['The dimension of the'...
    ' scale input does not match to the number of elements of the'...
    ' terminal. Please choose a valid input, or the scale is being ignored'])
end



% Populate the block's input port(s)

r = 0:number_pdo-1;
if strcmp(input_type, 'Vector Output')
        rv.PortConfig.input.pdo = [zeros(numel(r),4)];
        rv.PortConfig.input.pdo(:,2) = [r];
else
    for k = 1:number_pdo
        rv.PortConfig.input(k).pdo = [0, r(k), 0, 0];
    end
end
