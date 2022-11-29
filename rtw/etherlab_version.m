function v = etherlab_version
    % Return a version string for EtherLab, possibly appended
    % with an RCS ID
    % e.g. '2.3.4 (77cb20e)'

    p = ver('etherlab');
    v = p.Version;

    if isempty(strmatch(p.Release, strvcat('()', '(tarball)')))
        v = [v, ' ', p.Release];
    end
