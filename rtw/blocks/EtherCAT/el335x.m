function rv = el335x(method, varargin)

rv = [];

switch lower(method)
case 'config'

    rv = slave_models('sc',varargin{1},varargin{2},varargin{3});
    return

case 'check'
    % If UserData.SlaveConfig does not exist, this is an update
    % Convert this block and return

    ud = get_param(gcbh, 'UserData');

    if ~all(isfield(ud,{'product','revision'}))
        % Apparantly this slave has never been configured
        product = slave_models('product',get_param(gcbh,'model'));
        set_param(gcbh, 'UserDataPersistent', 'on');

    else
        % fields UserData.product and .revision exist

        % Get slave based on product code and revision
        product1 = slave_models('product', ud.product, ud.revision);

        % Get slave based on selected model
        product2 = slave_models('product', get_param(gcbh,'model'));

        if ~any([isempty(product1),isempty(product2)])
            % product1 and product2 are valid
            if isequal(product1,product2)
                %disp('ok')
                return
            end

            if ~strcmp(product1{1},product2{1})
                % Product names do not match, but the revision still
                % exists. Use the new product name
                product = product1;

                % The slave has a new name
                disp(sprintf('%s: Renaming device from %s to %s', ...
                        gcb, product2{1}, product1{1}))
            else
                % Product name still exists, so use the new product
                product = product2;

                % The slave has a new name
                disp(sprintf('%s: Using new device Product Code and Revision',...
                        gcb));
            end


        elseif ~isempty(product1)
            product = product1;
            disp(sprintf('%s: Using new device name %s', gcb, product{1}));
        elseif ~isempty(product2)
            product = product2;
            disp(sprintf('%s: Using new device %s', gcb, product{1}));
        else
            error('el335x:EExist', ...
                    '%s: No device could be found for name %s', ...
                    gcb, get_param(gcbh,'model'));
            return;
        end
    end

    set_param(gcbh, 'model', product{1});

    ud.product = product{2};
    ud.revision = product{3};
    ud.Update = @el335x;
    set_param(gcbh, 'UserData', ud);

case 'update'
    update_devices(varargin{1}, slave_models('models'));

case 'update-ui'
    dc = get_slave_config;
    slave_dc('ui',dc);
    el3351 = strcmp(get_param(gcbh,'model'), 'EL3351');
    update_mask_state('MaskEnables', {'rmb', 'control'}, ~el3351);

otherwise
    display([gcb, ': Unknown method ', method])
end

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function rv = slave_models(method,varargin)

%   Model       ProductCode          Revision          N, IP, DC
models = {...
  'EL3351',      hex2dec('0d1c3052'), hex2dec('00140000'),  [];
  'EL3356',      hex2dec('0d1c3052'), hex2dec('00140000'),   1;
  'EL3356-0010', hex2dec('0d1c3052'), hex2dec('0014000a'), 1:3;
};

rv = [];

switch method
case 'product'
    switch nargin
    case 3
        % Called with ProductCode and RevisionNo
        pos = cell2mat(models(:,2)) == varargin{1}...
            & cell2mat(models(:,3)) == varargin{2};
        rv = models(pos,:);
    
    case 2
        % Called with Model Name
        rv = models(strcmp(models(:,1),varargin{1}),:);
    end

case 'models'
    % Return a list of model names, obsolete ones at the end
    fields = models(:,1);
    obsolete = cellfun(@length, fields) > 11;
    rv = vertcat(sort(fields(~obsolete)), sort(fields(obsolete)));

case 'sc'
    % Do slave configuration
    % Called with (ModelName, GuiConfiguration)

    product = models(strcmp(models(:,1),get_param(gcbh,'model')),:);
    if isempty(product)
        return;
    end
    
    [config,dc,scale] = deal(varargin{:});

    rv.SlaveConfig.vendor = 2;
    rv.SlaveConfig.description = product{1};
    rv.SlaveConfig.product = product{2};
    rv.SlaveConfig.revision = product{3};
    [rv.SlaveConfig.sm, pdo, status_pdo, ctrl_pdo, rv.SlaveConfig.dc] = ...
        get_slave_config(config.control, config.status, config.rmb, dc);
    
    rv.PortConfig.input = struct('pdo',ctrl_pdo,'pdo_data_type',1001);

    port = 0;

    port = port + 1;
    rv.PortConfig.output(port).pdo = pdo{1};
    rv.PortConfig.output(port).pdo_data_type = 1032;
    if numel(scale.gain) > 0
        rv.PortConfig.output(port).gain = {'Gain1', scale.gain(1)};
        rv.PortConfig.output(port).full_scale = 2^31;
    end
    if numel(scale.offset) > 0
        rv.PortConfig.output(port).offset = {'Offset1', scale.offset(1)};
        rv.PortConfig.output(port).full_scale = 2^31;
    end
    if numel(scale.tau) > 0
        rv.PortConfig.output(port).tau = {'Filter1', scale.tau(1)};
    end

    port = port + 1;
    if ~isempty(status_pdo)
        rv.PortConfig.output(port).pdo = status_pdo{1};
        rv.PortConfig.output(port).pdo_data_type = 1001;
    end

    if numel(pdo) > 1
        port = port + 1;
        rv.PortConfig.output(port).pdo = pdo{2};
        rv.PortConfig.output(port).pdo_data_type = 1032;
        if numel(scale.gain) > 1
            rv.PortConfig.output(port).gain = {'Gain2', scale.gain(2)};
            rv.PortConfig.output(port).full_scale = 2^31;
        end
        if numel(scale.offset) > 1
            rv.PortConfig.output(port).offset = {'Offset2', scale.offset(2)};
            rv.PortConfig.output(port).full_scale = 2^31;
        end
        if numel(scale.tau) > 1
            rv.PortConfig.output(port).tau = {'Filter2', scale.tau(2)};
        end

        port = port + 1;
        if ~isempty(status_pdo)
            rv.PortConfig.output(port).pdo = status_pdo{2};
            rv.PortConfig.output(port).pdo_data_type = 1001;
        end
    end
end

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function varargout = get_slave_config(varargin)
%% This is the database for the slave configuration
%% Without arguments, this function returns DC only
%% With 3 arguments [control,status,rmb]
%%       return [dc, pdo, ctrl_pdo, dc]

% This function requires the variables 
%  control: control input is used
%  rmb: only rmb value
%  status: status output ports
if nargin == 4
    [control,status,rmb] = deal(varargin{1:3});
else
    [control,status,rmb] = deal([]);
end

ctrl_pdo = [];
status_pdo = [];

switch get_param(gcbh, 'model');
case 'EL3351'
    sm = {{3, 1, { { hex2dec('1a00'), [ hex2dec('6000'),  1,  1;
                                        hex2dec('6000'),  2,  1;
                                        0              ,  0,  4;
                                        hex2dec('6000'),  7,  1;
                                        0              ,  0,  7;
                                        hex2dec('1800'),  7,  1;
                                        hex2dec('1800'),  9,  1;
                                        hex2dec('6000'), 17, 32] }, ...
                   { hex2dec('1a01'), [ hex2dec('6010'),  1,  1;
                                        hex2dec('6010'),  2,  1;
                                        0              ,  0,  4;
                                        hex2dec('6010'),  7,  1;
                                        0              ,  0,  7;
                                        hex2dec('1801'),  7,  1;
                                        hex2dec('1801'),  9,  1;
                                        hex2dec('6010'), 17, 32] }}}};

    pdo = { [0, 0, 7, 0], ...
            [0, 1, 7, 0] };

    if status
        status_pdo = { [0, 0, 0, 0;
                        0, 0, 1, 0;
                        0, 0, 3, 0], ...
                       [0, 1, 0, 0;
                        0, 1, 1, 0;
                        0, 1, 3, 0]};
    end

    dc = [];

case 'EL3356'
    if rmb

        sm = {{3, 1, { { hex2dec('1a01'), [ hex2dec('6000'), 17, 32]} }}};
        pdo = { [ 0, 0, 0, 0] };

        if status
            status_pdo = { [ 0, 1, 1, 0;
                             0, 1, 3, 0;
                             0, 1, 5, 0;
                             0, 1, 6, 0;
                             0, 1, 7, 0] };
            sm{1}{3}{2} = {hex2dec('1a00'), [ 0              ,  0,  1;
                                              hex2dec('6000'),  2,  1;
                                              0              ,  0,  1;
                                              hex2dec('6000'),  4,  1;
                                              0              ,  0,  2;
                                              hex2dec('6000'),  7,  1;
                                              hex2dec('6000'),  8,  1;
                                              hex2dec('6000'),  9,  1;
                                              0              ,  0,  4;
                                              hex2dec('1c32'), 32,  1;
                                              0              ,  0,  1;
                                              hex2dec('1800'),  9,  1]};

        end

    else %rmb
        if status
            sm = {{3, 1, { { hex2dec('1a04'), [ hex2dec('6010'),  1,  1;
                                                hex2dec('6010'),  2,  1;
                                                0              ,  0,  4;
                                                hex2dec('6010'),  7,  1;
                                                0              ,  0,  8;
                                                hex2dec('6010'), 16,  1;
                                                hex2dec('6010'), 17, 32]}, ...
                           { hex2dec('1a06'), [ hex2dec('6020'),  1,  1;
                                                hex2dec('6020'),  2,  1;
                                                0              ,  0,  4;
                                                hex2dec('6020'),  7,  1;
                                                0              ,  0,  8;
                                                hex2dec('6020'), 16,  1;
                                                hex2dec('6020'), 17, 32]} }}};

            pdo = { [ 0, 0, 6, 0], ...
                    [ 0, 1, 6, 0] };
            status_pdo = { [ 0, 0, 0, 0;
                             0, 0, 1, 0;
                             0, 0, 3, 0;
                             0, 0, 5, 0], ...
                           [ 0, 1, 0, 0;
                             0, 1, 1, 0;
                             0, 1, 3, 0;
                             0, 1, 5, 0] };
        else %status
            sm = {{3, 1, { { hex2dec('1a05'), [ hex2dec('6010'), 17, 32 ]}, ...
                           { hex2dec('1a07'), [ hex2dec('6020'), 17, 32 ]} }}};
            pdo = { [ 0, 0, 0, 0], ...
                    [ 0, 1, 0, 0] };
        end %status
    end %rmb

    if control
        sm{2} = {2, 0, {{hex2dec('1600'), [ hex2dec('7000'),  1,  1;
                                            hex2dec('7000'),  2,  1;
                                            hex2dec('7000'),  3,  1;
                                            0              ,  0,  1;
                                            hex2dec('7000'),  5,  1;
                                            0              ,  0, 11; ] }}};
        ctrl_pdo = [1, 0, 0, 0;
                    1, 0, 1, 0;
                    1, 0, 2, 0;
                    1, 0, 4, 0];
    end %control

    dc = { 'SM-Synchron', repmat(0,1,10) };

case 'EL3356-0010'
    if rmb
        sm = {{3, 1, { { hex2dec('1a01'), [ hex2dec('6000'), 17, 32]}}}};
        pdo = { [ 0, 0, 0, 0] };

        if status
            status_pdo = { [ 0, 1, 1, 0;
                             0, 1, 3, 0;
                             0, 1, 5, 0;
                             0, 1, 6, 0;
                             0, 1, 7, 0] };
            sm{1}{3}{2} = {hex2dec('1a00'), [ 0              ,  0,  1;
                                              hex2dec('6000'),  2,  1;
                                              0              ,  0,  1;
                                              hex2dec('6000'),  4,  1;
                                              0              ,  0,  2;
                                              hex2dec('6000'),  7,  1;
                                              hex2dec('6000'),  8,  1;
                                              hex2dec('6000'),  9,  1;
                                              0              ,  0,  4;
                                              hex2dec('1c32'), 32,  1;
                                              0              ,  0,  1;
                                              hex2dec('1800'),  9,  1]};
        end

    else %rmb
        if status
            sm = {{3, 1, { { hex2dec('1a04'), [ hex2dec('6010'),  1,  1;
                                                hex2dec('6010'),  2,  1;
                                                0              ,  0,  4;
                                                hex2dec('6010'),  7,  1;
                                                0              ,  0,  1;
                                                0              ,  0,  7;
                                                hex2dec('6010'), 16,  1;
                                                hex2dec('6010'), 17, 32]}, ...
                           { hex2dec('1a06'), [ hex2dec('6020'),  1,  1;
                                                hex2dec('6020'),  2,  1;
                                                0              ,  0,  4;
                                                hex2dec('6020'),  7,  1;
                                                0              ,  0,  1;
                                                0              ,  0,  7;
                                                hex2dec('6020'), 16,  1;
                                                hex2dec('6020'), 17, 32]} }}};

            pdo = { [ 0, 0, 7, 0], ...
                    [ 0, 1, 7, 0] };
            status_pdo = { [ 0, 0, 0, 0;
                             0, 0, 1, 0;
                             0, 0, 3, 0;
                             0, 0, 6, 0], ...
                           [ 0, 1, 0, 0;
                             0, 1, 1, 0;
                             0, 1, 3, 0;
                             0, 1, 6, 0] };
        else %status
            sm = {{3, 1, { { hex2dec('1a05'), [ hex2dec('6010'), 17, 32 ]}, ...
                           { hex2dec('1a07'), [ hex2dec('6020'), 17, 32 ]} }}};
            pdo = { [ 0, 0, 0, 0], ...
                    [ 0, 1, 0, 0] };
        end %status
    end %rmb

    if control
        sm{2} = {2, 0, {{hex2dec('1600'), [ hex2dec('7000'),  1,  1;
                                            hex2dec('7000'),  2,  1;
                                            hex2dec('7000'),  3,  1;
                                            hex2dec('7000'),  4,  1;
                                            hex2dec('7000'),  5,  1;
                                            0              ,  0, 11; ] }}};
        ctrl_pdo = [1, 0, 0, 0;
                    1, 0, 1, 0;
                    1, 0, 2, 0;
                    1, 0, 3, 0;
                    1, 0, 4, 0];
    end %control

    dc = { 'SM-Synchron', repmat(0,1,10);
           'DC-Synchron', [ hex2dec('320'), 0,1,  0,0,0, 0,1, 0,0 ];
           'DC-Synchron (input based)', ...
                          [ hex2dec('320'), 0,1,  0,0,1, 0,1, 0,0 ] };

end

if ~nargin
    varargout = {dc};
else
    varargout = {sm, pdo, status_pdo, ctrl_pdo, slave_dc('config',dc,varargin{4})};
end

return
