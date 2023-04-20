from __future__ import print_function, unicode_literals, division
import argparse
import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim
from torchvision import datasets, transforms
from torch.optim.lr_scheduler import StepLR
import os
from pathlib import Path
import cv2 as cv
import numpy as np


# class MLP(nn.Module):
#     def __init__(self, inputs, num_layers, outputs):
#         super(MLP, self).__init__()
#         self.num_layers = num_layers
#         self.fc1 = nn.Linear(8192, 256)
#         self.fc2 = nn.Linear(256, 128)
#         self.fc3 = nn.Linear(128,64)
#         self.fc4 = nn.Linear(64,32)
#         self.fc5 = nn.Linear(32,10)
#         self.mlp = nn.MLP()
    
#     def forward(self):

def main():
   
    #read in the dataset
    dataset = pd.read_csv("/home/mikailg/ECE8650/optimizing_checkpoint_restart_project/training_data/combined_training_data.csv")
    sns.countplot(x = 'nfiles', data=dataset)
    dataset['strategy'] = dataset['strategy'].astype(category)
    print(dataset)


if __name__ == '__main__':
    main()