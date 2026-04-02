module top (
    input  wire clk_100mhz,
    input  wire trig_in,
    output reg  out1,
    output reg  out2
);

    //==================================================//
    //= 250 MHz clock from Vivado Clocking Wizard      =//
    //==================================================//
    wire clk_250mhz;
    wire clk_locked;

    clk_wiz_0 u_clk_wiz (
        .clk_in1 (clk_100mhz),
        .clk_out1(clk_250mhz),
        .locked  (clk_locked)
    );

    //==================================================//
    //= Synchronize trigger into 250 MHz domain        =//
    //==================================================//
    reg trig_ff1 = 1'b0;
    reg trig_ff2 = 1'b0;

    always @(posedge clk_250mhz) begin
        trig_ff1 <= trig_in;
        trig_ff2 <= trig_ff1;
    end

    wire trig_rise = trig_ff1 & ~trig_ff2;

    //==================================================//
    //= Pulse sequence state machine                   =//
    //==================================================//
    localparam [1:0]
        IDLE      = 2'd0,
        STEP_4NS  = 2'd1,
        STEP_12NS = 2'd2,
        OUT2_HOLD = 2'd3;

    reg [1:0] state = IDLE;
    reg [7:0] hold_count = 8'd0;

    localparam integer OUT2_WIDTH_CYCLES = 10;  // 40 ns example

    always @(posedge clk_250mhz) begin
        if (!clk_locked) begin
            state      <= IDLE;
            out1       <= 1'b0;
            out2       <= 1'b0;
            hold_count <= 8'd0;
        end else begin
            case (state)
                IDLE: begin
                    out1       <= 1'b0;
                    out2       <= 1'b0;
                    hold_count <= 8'd0;

                    if (trig_rise) begin
                        // wait one 250 MHz cycle ~ 4 ns
                        state <= STEP_4NS;
                    end
                end

                STEP_4NS: begin
                    // now 4 ns after trigger recognition
                    out1  <= 1'b1;
                    out2  <= 1'b0;
                    state <= STEP_12NS;
                end

                STEP_12NS: begin
                    // now 12 ns after trigger recognition
                    // out1 has been high for 8 ns total
                    out1       <= 1'b0;
                    out2       <= 1'b1;
                    hold_count <= 8'd0;
                    state      <= OUT2_HOLD;
                end

                OUT2_HOLD: begin
                    out1 <= 1'b0;
                    out2 <= 1'b1;

                    if (hold_count == OUT2_WIDTH_CYCLES - 1) begin
                        out2       <= 1'b0;
                        hold_count <= 8'd0;
                        state      <= IDLE;
                    end else begin
                        hold_count <= hold_count + 1'b1;
                    end
                end

                default: begin
                    state      <= IDLE;
                    out1       <= 1'b0;
                    out2       <= 1'b0;
                    hold_count <= 8'd0;
                end
            endcase
        end
    end

endmodule