
            local s = state("score")
            if s == nil then return -1 end
            
            local val = s:get()
            val = val + 10
            s:set(val)
            return val
        