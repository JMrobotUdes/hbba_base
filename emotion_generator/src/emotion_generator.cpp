#include "emotion_generator.h"

EmotionGenerator::EmotionGenerator(ros::NodeHandle * n, std::string nodeName)
{
	timer_ = n->createTimer(ros::Duration(1),
			&EmotionGenerator::timerCB, this);
	timerDecay_ = n->createTimer(ros::Duration(2),
			&EmotionGenerator::emotionDecayCB, this);

	n_ = n;
	nodeName_ = nodeName;

	pubEmotion = n_->advertise<hbba_msgs::EmotionIntensities>("emotions",100,true);
	pubEmotionFaceExpression = n_->advertise<emotions_msgs::Intensity>("emo_intensity",100,true);

	ros::NodeHandle np("~");

	np.param("debug_emotion_generator",debugEmotionGenerator_,false);
	np.param("emotion_decay",emotionDecay_,0.01);
	if(emotionDecay_ > 1 || emotionDecay_ < 0)
	{
		emotionDecay_ = 0.01;
		ROS_WARN("emotion decay must be between 0 and 1");
	}

	ROS_INFO("debug emotion generator %i",debugEmotionGenerator_);
	if(debugEmotionGenerator_)
	{
		pubDebugAnger = n_->advertise<emotions_msgs::Intensity>("debug_anger",100,true);
		pubDebugJoy = n_->advertise<emotions_msgs::Intensity>("debug_joy",100,true);
		pubDebugGotoExp = n_->advertise<std_msgs::Bool>("debug_goto_exp",100,true);
		pubDebugGotoDesire = n_->advertise<std_msgs::Bool>("debug_goto_des",100,true);
	}

}

void EmotionGenerator::eventsCallback(const hbba_msgs::Event& msg)
{
	//ROS_INFO("emotionGenerator event %i %s %s",msg.type, msg.desire.c_str(), msg.desire_type.c_str());

	switch(msg.type)
	{
	case hbba_msgs::Event::EXP_ON:
	{
		exploitedDesires[msg.desire_type] = true;
		if(msg.desire_type.compare("GoTo") == 0)
		{
			ROS_INFO("exp on GoTo");
		}
		break;
	}
	case hbba_msgs::Event::EXP_OFF:
	{
		exploitedDesires[msg.desire_type] = false;
		if(msg.desire_type.compare("GoTo") == 0)
		{
			ROS_INFO("exp off GoTo");
		}
		break;
	}
	case hbba_msgs::Event::DES_ON:
	{
		activeDesires[msg.desire_type] = true;
		if(msg.desire_type.compare("GoTo") == 0)
		{
			ROS_INFO("des on GoTo");
		}
		break;
	}
	case hbba_msgs::Event::DES_OFF:
	{
		activeDesires[msg.desire_type] = false;
		if(msg.desire_type.compare("GoTo") == 0)
		{
			ROS_INFO("des off GoTo");
		}
		break;
	}
	case hbba_msgs::Event::INT_ON:
	{
		if(msg.desire_type.compare("GoTo") == 0)
		{
			ROS_INFO("int on GoTo");
		}
		break;
	}
	case hbba_msgs::Event::INT_OFF:
	{
		if(msg.desire_type.compare("GoTo") == 0)
		{
			ROS_INFO("int off GoTo");
		}
		break;
	}
	}

	if(!msg.desire_type.empty() && emotionMatrix.count(msg.desire_type) == 0)
	{
		//ROS_INFO("emotionMatrix %s",paramDesire.c_str());
		getEmotionDesireRelation(msg.desire_type);
		getEmotionDesireRelation("not_"+msg.desire_type);

	}

}

void EmotionGenerator::getEmotionDesireRelation(std::string desireType)
{
	XmlRpc::XmlRpcValue emotionParam;
	std:: string paramServerDesire = n_->getNamespace() + nodeName_ + "/" + desireType;
	n_->getParam(paramServerDesire, emotionParam);

	std::map<std::string,double> mapEmotion;

	for (std::map<std::string, XmlRpc::XmlRpcValue>::iterator it = emotionParam.begin(); it != emotionParam.end(); it++)
	{
		double emotionFactor = 0;
		if((*it).second.getType() == XmlRpc::XmlRpcValue::TypeInt)
		{
			int value = static_cast<int>((*it).second);
			//ROS_INFO("factor %s %s %i",msg.desire_type.c_str(),(*it).first.c_str(), value);
			emotionFactor = (double)value;

		}
		else if ((*it).second.getType() == XmlRpc::XmlRpcValue::TypeDouble)
		{
			emotionFactor = static_cast<double>((*it).second);
			//ROS_INFO("factor %s %s %5.2f",msg.desire_type.c_str(),(*it).first.c_str(), emotionFactor);
		}

		mapEmotion[(*it).first] = emotionFactor;

		if(emotionIntensities.count((*it).first) == 0)
		{
			//initialize this emotion
			emotionIntensities[(*it).first] = 0;
		}

	}
	if(mapEmotion.size() > 0)
		emotionMatrix[desireType] = mapEmotion;
}

void EmotionGenerator::timerCB(const ros::TimerEvent&)
{
	generateEmotions();

	hbba_msgs::EmotionIntensities emotions;
	emotions_msgs::Intensity intensity;
	//publish emotionIntensities
	for(std::map<std::string,double>::iterator it = emotionIntensities.begin() ; it != emotionIntensities.end() ; it++)
	{
		intensity.name = (*it).first;
		intensity.value = (*it).second;
		emotions.emotion.push_back(intensity);

		//for debug purposes
		if(intensity.name.compare("Anger") == 0 && debugEmotionGenerator_ > 0)
		{
			pubDebugAnger.publish(intensity);
		}
		else if(intensity.name.compare("Joy") == 0 && debugEmotionGenerator_ > 0)
		{
			pubDebugJoy.publish(intensity);
		}

		//intensity.value /= 100; //face_expresion need intensity between 0 and 1
		pubEmotionFaceExpression.publish(intensity);
	}

	//debug desire state vs exploited state
	for(std::map<std::string,bool>::iterator it = activeDesires.begin() ; it != activeDesires.end() ; it++)
	{
		if((*it).first.compare("GoTo") == 0)
		{
			std_msgs::Bool isGotoDesired;
			isGotoDesired.data = (*it).second;
			pubDebugGotoDesire.publish(isGotoDesired);
		}
	}
	for(std::map<std::string,bool>::iterator it = exploitedDesires.begin() ; it != exploitedDesires.end() ; it++)
	{
		if((*it).first.compare("GoTo") == 0)
		{
			std_msgs::Bool isGotoExploited;
			isGotoExploited.data = (*it).second;
			pubDebugGotoExp.publish(isGotoExploited);
		}
	}

	pubEmotion.publish(emotions);
}

void EmotionGenerator::generateEmotions()
{
	//verify each active desires if they are exploited
	for(std::map<std::string,bool>::iterator it = activeDesires.begin() ; it != activeDesires.end() ; it++)
	{
		std::string desires = (*it).first;
		if((*it).second && exploitedDesires[desires])
		{
			//emotion are influence according to a matrix defined in yaml file
			for(std::map<std::string,double>::iterator it = emotionMatrix[desires].begin() ; it!= emotionMatrix[desires].end() ; it++)
			{
				std::string emotion = (*it).first;
				double emotionModulation = (*it).second;
				double emotionIntensity = emotionIntensities[emotion];
				emotionIntensity += emotionModulation;
				if(emotionIntensity > 1)
					emotionIntensity = 1;
				else if(emotionIntensity < 0)
					emotionIntensity = 0;

				emotionIntensities[emotion] = emotionIntensity; //update in map
			}
		}
		//if there is a desire but its not exploited, the emotion are influence according to the inverse values in matrix
		else if( ((*it).second && exploitedDesires.count(desires) == 0) || ((*it).second && exploitedDesires[desires] == false) )
		{
			desires = "not_"+desires;
			for(std::map<std::string,double>::iterator it = emotionMatrix[desires].begin() ; it!= emotionMatrix[desires].end() ; it++)
			{
				std::string emotion = (*it).first;
				double emotionModulation = (*it).second;
				double emotionIntensity = emotionIntensities[emotion];
				emotionIntensity += emotionModulation;
				if(emotionIntensity > 1)
					emotionIntensity = 1;
				else if(emotionIntensity < 0)
					emotionIntensity = 0;

				emotionIntensities[emotion] = emotionIntensity; //update in map

			}
		}
	}
}

void EmotionGenerator::emotionDecayCB(const ros::TimerEvent&)
{
	//reduce every emotion intensity by emotionDecay_
	for(std::map<std::string,double>::iterator it = emotionIntensities.begin() ; it != emotionIntensities.end() ; it++)
	{
		(*it).second -= emotionDecay_;
		if((*it).second < 0)
		{
			(*it).second = 0;
		}
	}
}
