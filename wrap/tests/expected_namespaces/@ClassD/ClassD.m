classdef ClassD
  properties
    self = 0
  end
  methods
    function obj = ClassD(varargin)
      if nargin == 0, obj.self = new_ClassD_(); end
      if nargin ~= 13 && obj.self == 0, error('ClassD constructor failed'); end
    end
    function display(obj), obj.print(''); end
    function disp(obj), obj.display; end
  end
end