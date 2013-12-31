function count()
    var c = 0;

    function ()
        c = c + 1
    end
end;

var c = (count());
c(); # 1
c(); # 2
c(); # 3
c(); # 4
c()  # 5
