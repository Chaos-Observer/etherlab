classdef EtherCATInfoSlave < XmlNode
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods
        %------------------------------------------------------------------
        function slave = EtherCATInfoSlave(s)
            slave = slave@XmlNode(s);
        end

        %------------------------------------------------------------------
        function rv = Name(s)
            type = s.getFirstNode('Type');
            rv = char(type.getTextContent);
        end

        %------------------------------------------------------------------
        function rv = ProductCode(s)
            type = s.getFirstNode('Type');
            pc = type.getAttribute('ProductCode');
            rv = EtherCATInfo.hexDecValue(char(pc));
        end

        %------------------------------------------------------------------
        function rv = hideTypes(s)
            rv = cellfun(@(x) EtherCATInfo.hexDecValue(...
                                x.getAttribute('RevisionNo')), ...
                         s.getNodes('HideType'));
        end

        %------------------------------------------------------------------
        function rv = getObject(s)
            dict = s.getFirstNode('Profile').getFirstNode('Dictionary');
            rv = cellfun(@(o) struct('node', o, ...
                                     'index', o.getFirstNode('Index').getTextContent, ...
                                     'type', o.getFirstNode('Type').getTextContent, ...
                                     'name', o.getFirstNode('Name').getTextContent), ...
                        dict.getFirstNode('Objects').getNodes('Object'));
        end

        %------------------------------------------------------------------
        function rv = getTypes(s)
            dict = s.getFirstNode('Profile').getFirstNode('Dictionary');
            rv = cellfun(@(t) struct('node', t, ...
                                     'name', t.getFirstNode('Name').getTextContent), ...
                        dict.getFirstNode('DataTypes').getNodes('DataType'));
        end

        %------------------------------------------------------------------
        function rv = RevisionNumber(s)
            type = s.getFirstNode('Type');
            pc = type.getAttribute('RevisionNo');
            rv = EtherCATInfo.hexDecValue(char(pc));
        end
    end
end
