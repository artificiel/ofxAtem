#include "ofMain.h"
#include "ofxAtem.h"
#include "ofxVectorGuiWidgets.hpp"
#include "ofxFontStash.h"

using namespace std::string_literals;


class Assignment: public ofxVectorGuiWidgets::Button {
    
    std::weak_ptr<ofxAtem::InputMonitor> input_;
    
public:
    Assignment(std::weak_ptr<ofxAtem::InputMonitor> input, std::string label)
    : ofxVectorGuiWidgets::Button(label, false, false, true)
    , input_(input) {
    };
    
    auto get_assigned_input() { return input_ ; }
	
	virtual ~Assignment() {};

};

class AssignedOutput {
    
public:
    std::weak_ptr<ofxAtem::InputMonitor> input_;
    std::weak_ptr<ofxAtem::InputMonitor> output_;
    
    std::vector<std::shared_ptr<Assignment>> assignements_;
    
    AssignedOutput(std::weak_ptr<ofxAtem::InputMonitor> output): output_(output) {};
    
    auto get_name() {
        if (auto output = output_.lock()) {
            return output->getName();
        } else {
            return "<dangling output>"s;
        }
    }
    
    auto getPortTypeName() {
        if (auto output = output_.lock()) {
            return output->getPortTypeName();
        } else {
            return "<dangling output>"s;
        }
    }
};

class ThreadedAtemController : public ofxAtem::Controller, public ofThread {
    
    std::string ip_;
    ofThreadChannel<bool> fresh_model_;


    void threadedFunction() override {
        while (isThreadRunning()) {
            if (!isConnected()) {
                ofxAtem::Controller::connect(ip_);
                if (isConnected()) {
                    build_model();
                }
            }
            ofSleepMillis(1000);
        }
    }

public:
	
	virtual ~ThreadedAtemController() {};
    std::mutex updating_;

    std::vector<std::weak_ptr<ofxAtem::InputMonitor>> inputs_;
    std::vector<std::shared_ptr<AssignedOutput>> outputs_;
    
    auto has_new_model() {
        bool new_model = false;
        while(fresh_model_.tryReceive(new_model)) { new_model = true; }
        return new_model;
    }
    
    void build_model() {
        std::scoped_lock lock(updating_);
        for (const auto & input: getInputMonitors()) {
            if (input->getPortType() == bmdSwitcherPortTypeAuxOutput) {
                auto output =make_shared<AssignedOutput>(input);
                outputs_.push_back(output);
            } else {
                inputs_.push_back(input);
            }
        }
        
        size_t output_index = 0;
        for (const auto & output: outputs_) {
            output->assignements_.clear();
            
            for (const auto & i: inputs_) {
                BMDSwitcherInputId input_id;
                
                auto label = "dangling input"s;
                if (auto input = i.lock()) {
                    label = input->getName();
                    input->input()->GetInputId(&input_id);
                }
                label += "->";
                label += output->get_name();
                auto ass = make_shared<Assignment>(i, label);
                output->assignements_.push_back(ass);
                output->assignements_.back()->callbacks_["atem"] = [&,ass, output, input_id, output_index](bool v) {
                    ofLogNotice("got button") << input_id << " v=" << v;
                    if (v==0) {
						ofLogNotice("v==0") << v;
//                        ass->set(1); // prevent auto-toggle disable by itself
                    } else {
                        for (const auto & a : output->assignements_) {
                            if (a != ass)  {
//								ofLogNotice("this is not me, so 0 (silent");
//                                a->set(v, true); // could check if on but no side-effects on call
                            } else {
								ofLogNotice("this is  me, so ask the switch to change") << output_index << " " << input_id;
                                mSwitcherInputAuxList[output_index]->SetInputSource(input_id);
								ass->set(v, true); // prevent auto-toggle disable by itself
                           }
                        }
                    }
                };
            }
            output_index++;
        }
        fresh_model_.send(true);
    }
    
    auto getIP() { return ip_;}
    auto connect(std::string ip) {
        ip_ = ip;
        startThread();
    }
};

class ofApp : public ofBaseApp{
    
    ofxFontStash  matrix_font_;
    
	std::vector<std::string> desired_inputs_ = {
		"Black",
		"Dactylo",
		"Ile",
		"Cam PTZ",
		"Cam Stand",
		"Cam Top",
		"Cam Top",
		"Linux FX",
		"Linux PRE",
		"Mac",
		"Color Bars",
		"Color 1",
	};
	
	std::vector<std::string> desired_outputs_ = {
		"Linux",
		"Projecteur",
		"Cathodiques",
		"Screen 5 (7\")",
		"Screen 4 (24\")",
		"Screen 3 (43\")",
		"Screen 2 (55\")",
		"Screen 1 (75\")",
		"Preview",
		"Mac",
	};
	
    ThreadedAtemController atem;
    
    ofFbo text_overlay_;
    
    float font_scale_ = 18;
public:
    void setup()
    {
        ofEnableAlphaBlending();
        ofSetVerticalSync(true);
        ofSetFrameRate(60);
        matrix_font_.setup("fonts/Roboto-Medium.ttf", 1.0, 1024, false, 8, 2.0);
        matrix_font_.setSize(16);
//        matrix_font_.setup("fonts/noto-sans/NotoSans-Regular.ttf", 1.0, 1024, false, 8, 1.5);

        atem.connect("192.168.101.238");
    }
    
    float longuest_input_ = 0;
    float longuest_output_ = 0;
    void update() {
        
        if (text_overlay_.getWidth() != ofGetWidth()) {
            text_overlay_.allocate(ofGetWidth(), ofGetHeight(), GL_RGBA);
        }
        atem.update();
        if (atem.has_new_model()) {
            ofSetWindowTitle(atem.getProductName() + " @ " + atem.getIP() + " :: Auxiliary Outputs Matrix");
            ofLogNotice("new model!");
            longuest_input_ = 0;
            for (const auto & i: atem.inputs_) {
                if (auto input = i.lock()) {
                    auto w =matrix_font_.stringWidth(input->getName()) ;
                    ofLogNotice("s") << w;
                    if (w > longuest_input_) longuest_input_ = w;
                }
            }
            longuest_output_ = 0;
            for (const auto & output: atem.outputs_) {
                auto w =matrix_font_.stringWidth(output->get_name()) ;;
                if (w > longuest_output_) longuest_output_ = w;
            }
        }
        
        // should be on notification only but cannot figure it out
        size_t idx = 0;
        for (const auto & aux: atem.outputs_) {
            auto atem_out =  atem.getAuxOutputs()[idx];
            auto my_out = aux->output_;
            if (auto output = my_out.lock()) {
                for (const auto & ass: aux->assignements_) {
                    if (!ass->get_assigned_input().expired()) {
                        if (auto input = ass->get_assigned_input().lock()) {
                            BMDSwitcherInputId inputId ;
                            input->input()->GetInputId(&inputId);
                            
                            if (inputId == atem_out) {
                                ass->set(1);
                            } else {
                                ass->set(0);
                            }
                        }
                    }
                }
            }
            idx++;
        }
    }
    void draw()
    {
        ofClear(0);
        
        
        if (0) {
            if (!atem.isConnected()) {
                ofDrawBitmapStringHighlight("NOT CONNECTED @ " + atem.getIP(), 10, 20);
            } else {
                ofPushStyle();
                ofSetColor(0, 255, 0);
                ofDrawBitmapStringHighlight("CONNECTED @ " + atem.getIP(), 10, 20);
                ofPopStyle();
            }
            ofDrawBitmapStringHighlight(atem.getProductName(), 10, 20);
            ofDrawBitmapStringHighlight(atem.getVideoModeString(), 10, 40);
            
            ofDrawBitmapStringHighlight("Program : " + ofToString(atem.getProgramId()), 10, 60);
            ofDrawBitmapStringHighlight("Preview : " + ofToString(atem.getPreviewId()), 10, 80);
            
            ofDrawBitmapStringHighlight(ofToString(ofGetFrameRate()), 10, 100);
            
            
            ofPushMatrix();
            ofTranslate(ofGetWidth()-300,200);
            for (size_t i=0; i < atem.getAuxOutputs().size(); i++) {
                
                ofDrawBitmapString(ofToString(i) + " : " + ofToString(atem.getAux(i)), 10, 80);
                //            ofDrawBitmapString(atem.getInputMonitors()[atem.getAux(i)]->getName(), 100, 80);
                
                ofTranslate(0,10);
            }
            ofPopMatrix();
        }
        
        if (atem.updating_.try_lock()) {
//            text_overlay_.begin();
//            ofClear(0,0,200,0);
            if (0) {
                ofPushMatrix();
                ofTranslate(300,10);
                
                for (const auto & i: atem.inputs_) {
                    if (auto input = i.lock()) {
                        
                        ofDrawBitmapString(input->getName(), 0, 0);
                        try {
                            ofDrawBitmapString(input->getPortTypeName(), 100, 0);
                        } catch (...) {
                            ofDrawBitmapString("unknown type", 100, 0);
                        }
                        ofTranslate(0,10);
                    }
                }
                ofPopMatrix();
                ofPushMatrix();
                ofTranslate(600,0);
                for (const auto & output: atem.outputs_) {
                    
                    ofDrawBitmapString(output->get_name(), 0, 0);
                    try {
                        ofDrawBitmapString(output->getPortTypeName(), 100, 0);
                    } catch (...) {
                        ofDrawBitmapString("unknown type", 100, 0);
                    }
                    ofTranslate(0,10);
                    
                }
                ofPopMatrix();
            }
        
			std::vector<std::weak_ptr<ofxAtem::InputMonitor>> desired_inputs;
			for (const auto & input: atem.inputs_) {
				if (auto i = input.lock()) {
					if (ofContains(desired_inputs_, i->getName())) desired_inputs.push_back(input);
				}
			}
			
			std::vector<std::shared_ptr<AssignedOutput>>  desired_outputs;
			for (const auto & output: atem.outputs_) {
				if (ofContains(desired_outputs_, output->get_name())) desired_outputs.push_back(output);
			}
			
            auto canvas_w = ofGetWidth() - longuest_output_ - 20;
            auto w = canvas_w / desired_inputs.size();
            
            ofPushMatrix();
            ofTranslate(0.5, 0.5);
            ofTranslate(10, ofGetHeight()-(w*desired_outputs.size())-10);
            ofPushStyle();
            bool first_line = true;
            for (const auto & output: desired_outputs) {
				if (ofContains(desired_outputs_, output->get_name())) {
					ofPushMatrix();
					for (const auto & ass: output->assignements_) {
						if (auto input = ass->get_assigned_input().lock()) {
							if (ofContains(desired_inputs_, input->getName())) {
								
								if (first_line) {
									ofPushMatrix();
									ofTranslate(w/2,0);
									ofRotateDeg(-60);
									//                            ofScale(1.0/font_scale_);
									matrix_font_.drawString(input->getName(), 0,0);
									ofPopMatrix();
								}
								ass->draw(1,1,w-2,w-2);
								ofTranslate(w,0);
							} else {
							}
						}
					}
					//                ofScale(1.0/font_scale_);
					matrix_font_.drawString(output->get_name(), 5,(w/2) + (matrix_font_.getSize()/3));
					ofPopMatrix();
					ofTranslate(0,w);
					first_line = false;
//				} else {
//					ofLogNotice("reject output") << output->get_name();
				}
            }
            
            atem.updating_.unlock();

            ofPopStyle();
            ofPopMatrix();
            
//            text_overlay_.end();
//            text_overlay_.draw(0,0);
            
        } else {
            ofDrawBitmapString("UPDATING MODEL", 100, 10);
        }
//        ofPushMatrix();
//        ofNoFill();
//        ofTranslate(200,40);
//        ofRotateDeg(-60);
//        matrix_font_.draw("Matrix", 20, 0, 0);
//        ofPopMatrix();
    }
    
    void keyPressed(int key)
    {
        if (key >= '1' && key <= '8') {
            atem.setPreviewId(key - '0');
            atem.setAux(1, key-'0');
        }
        if (key == OF_KEY_RETURN) {
            atem.performCut();
        }
        if (key == 'a') {
            atem.performAuto();
        }
    
    }
};

//========================================================================
int main( ){
    ofSetupOpenGL(640,360,OF_WINDOW);            // <-------- setup the GL context
    
    // this kicks off the running of my app
    // can be OF_WINDOW or OF_FULLSCREEN
    // pass in width and height too:
    ofRunApp(new ofApp());
    
}
